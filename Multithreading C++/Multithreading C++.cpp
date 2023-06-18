
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

constexpr bool chunkMeasurementEnabled = true;
constexpr size_t workerCount = 4;
constexpr size_t chunkSize = 8'000;
constexpr size_t chunkCount = 100;
constexpr size_t lightIterations = 100;
constexpr size_t heavyIterations = 1'000;
constexpr size_t subsetSize = chunkSize / workerCount;
constexpr double probabilityHeavy = 0.15;

static_assert(chunkSize >= workerCount);
static_assert(chunkSize% workerCount == 0);

struct Task
{
    double val;
    bool isHeavy;
    unsigned int Process() const
    {
        const auto iterations = isHeavy ? heavyIterations : lightIterations;
        auto intermediate = val;
        for (size_t i = 0; i < iterations; i++)
        {
            const auto digits = unsigned int(std::abs(std::sin(std::cos(intermediate) * std::numbers::pi) * 10'000'000)) % 100'000;
            intermediate = double(digits) / 10'000;
        }
        return unsigned int(std::exp(intermediate));
    }
};

auto GenerateDataset()
{
    std::minstd_rand rne;
    std::uniform_real_distribution vDist{ 0.0, 2.0 * std::numbers::pi };
    std::bernoulli_distribution hDist{ probabilityHeavy };

    std::vector<std::array<Task, chunkSize>> chunks(chunkCount);

    for (auto& chunk : chunks)
    {
        std::ranges::generate(chunk, [&] {return Task{ .val = vDist(rne), .isHeavy = hDist(rne) };});
    }

    return chunks;
}

auto GenerateDatasetEven()
{
    std::minstd_rand rne;
    std::uniform_real_distribution vDist{ 0.0, 2.0 * std::numbers::pi };

    std::vector<std::array<Task, chunkSize>> chunks(chunkCount);

    for (auto& chunk : chunks)
    {
        std::ranges::generate(chunk, [&, acc = 0.0]() mutable {
            bool heavy = false;
            if ((acc += probabilityHeavy) >= 1.0)
            {
                acc -= 1.0;
                heavy = true;
            }
            return Task{ .val = vDist(rne), .isHeavy = heavy };});
    }
    return chunks;
}

auto GenerateDatasetStacked()
{
    auto chunks = GenerateDatasetEven();
    for (auto& chunk : chunks)
    {
        std::ranges::partition(chunk, std::identity{}, &Task::isHeavy);
    }
    return chunks;
}

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

struct ChunkTimingInfo
{
    std::array<float, workerCount> timeSpentPerThread;
    std::array<size_t, workerCount> numberOfHeavyItemsPerThread;
    float totalChunkTime;
};

int DoTest()
{
    //const auto chunks = GenerateDataset();
    //const auto chunks = GenerateDatasetEven();
    const auto chunks = GenerateDatasetStacked();

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
                cur.timeSpentPerThread[i] = workerPtrs[i]->GetWorkTime();
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
        std::ofstream csv{ "timings.csv", std::ios_base::trunc };
        for (size_t i = 0; i < workerCount; i++)
        {
            csv << std::format("work_{0:},idel_{0:},heavy_{0:},", i);
        }
        csv << "chunktime,total_idel,total_heavy\n";

        for (const auto& chunk : timings)
        {
            float totalIdle = 0.0f;
            size_t totalHeavy = 0;
            for (size_t i = 0; i < workerCount; i++)
            {
                const auto idle = chunk.totalChunkTime - chunk.timeSpentPerThread[i];
                const auto heavy = chunk.numberOfHeavyItemsPerThread[i];
                csv << std::format("{},{},{},", chunk.timeSpentPerThread[i], idle, heavy);
                totalIdle += idle;
                totalHeavy += heavy;
            }
            csv << std::format("{},{},{}\n", chunk.totalChunkTime, totalIdle, totalHeavy);
        }
    }
    return 0;
}

int main()
{
    return DoTest();
}