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

namespace Pre
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
    private:
        std::condition_variable cv;
        std::mutex mtx;
        std::unique_lock<std::mutex> lk;
        //shared memory
        int doneCount = 0;
    };

    class Worker
    {
    public:
        Worker(MasterControl* pMaster) : pMaster{ pMaster }, thread{ &Worker::Run_, this } {}
        ~Worker() { Kill(); }
        void SetJob(std::span<const Task> data)
        {
            {
                std::lock_guard lk{ mtx };
                input = data;
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
            for (auto& task : input)
            {
                accumulation += task.Process();
                if constexpr (chunkMeasurementEnabled)
                {
                    numHeavyItemsProcessed += task.isHeavy ? 1 : 0;
                }
            }
        }
        void Run_()
        {
            std::unique_lock lk{ mtx };
            while (true)
            {
                Timer timer;
                cv.wait(lk, [this] {return !input.empty() || dying;});
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
                input = {};
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
        std::span<const Task> input;
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
            for (size_t iSubset = 0; iSubset < workerCount; iSubset++)
            {
                workerPtrs[iSubset]->SetJob(std::span{ &chunk[iSubset * subsetSize], subsetSize });
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