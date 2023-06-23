#pragma once

#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <ranges>
#include <cmath>
#include <limits>
#include <thread>
#include <mutex>
#include <span>
#include <numbers>
#include <fstream>
#include <format>
#include "Timer.h"
#include "Constants.h"
#include "Task.h"
#include "Timing.h"

namespace Que
{
    class MasterControl
    {
    public:
        MasterControl() : lk{ mtx } {}
        void SignalDone()
        {
            bool needsNotification = false;
            {
                std::lock_guard lk{ mtx };
                doneCount++;
                needsNotification = doneCount == workerCount;
            }
            if (needsNotification)
            {
                cv.notify_one();
            }
        }
        void WaitForAllDone()
        {
            cv.wait(lk, [this] {return doneCount == workerCount;});
            doneCount = 0;
        }
        void SetChunk(std::span<const Task> chunk)
        {
            idx = 0;
            currentChunk = chunk;
        }
        const Task* GetTask()
        {
            std::lock_guard lk{ mtx };
            const auto i = idx++;
            if (i > chunkSize)
            {
                return nullptr;
            }
            return &currentChunk[i];
        }
    private:
        std::condition_variable cv;
        std::mutex mtx;
        std::unique_lock<std::mutex> lk;
        std::span<const Task> currentChunk;
        //shared memory
        int doneCount = 0;
        size_t idx = 0;
    };

    class Worker
    {
    public:
        Worker(MasterControl* pMaster) : pMaster{ pMaster }, thread{ &Worker::Run_, this } {}
        ~Worker() { Kill(); }
        void StartWork()
        {
            {
                std::lock_guard lk{ mtx };
                working = true;
            }
            cv.notify_one();
        }
        void Kill()
        {
            {
                std::lock_guard lk{ mtx };
                dying = true;
            }
            cv.notify_one();
        }
        unsigned int GetResult() const
        {
            return accumulation;
        }
        size_t GetNumHeavyItemsProcessed() const
        {
            return numHeavyItemsProcessed;
        }
        float GetWorkTime() const
        {
            return workTime;
        }
    private:
        void ProcessData_()
        {
            if constexpr (chunkMeasurementEnabled)
            {
                numHeavyItemsProcessed = 0;
            }
            while(auto pTask = pMaster->GetTask())
            {
                accumulation += pTask->Process();
                if constexpr (chunkMeasurementEnabled)
                {
                    numHeavyItemsProcessed += pTask->isHeavy ? 1 : 0;
                }
            }
        }
        void Run_()
        {
            std::unique_lock lk{ mtx };
            while (true)
            {
                Timer timer;
                cv.wait(lk, [this] {return working || dying;});
                if (dying)
                {
                    break;
                }
                if constexpr (chunkMeasurementEnabled)
                {
                    timer.Mark();
                }
                ProcessData_();
                if constexpr (chunkMeasurementEnabled)
                {
                    workTime = timer.Peek();
                }
                working = false;
                pMaster->SignalDone();
            }
        }
    private:
        MasterControl* pMaster;
        std::jthread thread;
        std::condition_variable cv;
        std::mutex mtx;
        //shared memory
        bool dying = false;
        bool working = false;
        unsigned int accumulation = 0;
        float workTime = -1.0f;
        size_t numHeavyItemsProcessed = 0;
    };

    int DoTest(Dataset chunks)
    {
        Timer chunkTimer;
        std::vector<ChunkTimingInfo> timings;
        timings.reserve(chunkCount);

        Timer totalTimer;
        totalTimer.Mark();

        MasterControl mctrl;
        std::vector<std::unique_ptr<Worker>> workerPtrs(workerCount);
        std::ranges::generate(workerPtrs, [pMctrl = &mctrl] {return std::make_unique<Worker>(pMctrl);});
        for (auto& chunk : chunks)
        {
            if constexpr (chunkMeasurementEnabled)
            {
                chunkTimer.Mark();
            }
            mctrl.SetChunk(chunk);
            for (auto& pWorker : workerPtrs)
            {
                pWorker->StartWork();
            }
            mctrl.WaitForAllDone();

            if constexpr (chunkMeasurementEnabled)
            {
                timings.push_back(ChunkTimingInfo{ .totalChunkTime = chunkTimer.Peek() });
                for (size_t i = 0; i < workerCount; i++)
                {
                    auto& cur = timings.back();
                    cur.numberOfHeavyItemsPerThread[i] = workerPtrs[i]->GetNumHeavyItemsProcessed();
                    cur.timeSpentWorkingPerThread[i] = workerPtrs[i]->GetWorkTime();
                }
            }
        }

        const auto t = totalTimer.Peek();
        std::cout << "Processing took " << t << " seconds" << std::endl;

        unsigned int finalResult = 0;
        for (const auto& w : workerPtrs)
        {
            finalResult += w->GetResult();
        }
        std::cout << "Result is " << finalResult << std::endl;

        if constexpr (chunkMeasurementEnabled)
        {
            WriteCSV(timings);
        }
        return 0;
    }
}