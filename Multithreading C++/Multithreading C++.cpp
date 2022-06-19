
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
#include "Timer.h"

constexpr size_t DATASET_SIZE = 50'000'000;

void ProcessDataset(std::span<int> set, int& sum)
{
    for (int num : set)
    {
        constexpr auto limit = (double)std::numeric_limits<int>::max();
        const auto x = (double)num / limit;
        sum += int(std::sin(std::cos(x)) * limit);
    }
}

class MasterControl
{
public:
    MasterControl(int workerCount) : lk{ mtx }, workerCount{ workerCount } {}
    void SignalDone()
    {
        {
            std::lock_guard lk{ mtx };
            doneCount++;
        }
        if (doneCount == workerCount)
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
    int workerCount;
    //shared memory
    int doneCount = 0;
};

class Worker
{
public:
    Worker(MasterControl* pMaster) : pMaster{ pMaster }, thread{ &Worker::Run_, this } {}
    void SetJob(std::span<int> data, int* pOut)
    {
        {
            std::lock_guard lk{ mtx };
            input = data;
            pOutput = pOut;
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
private:
    void Run_()
    {
        std::unique_lock lk{ mtx };
        while (true)
        {
            cv.wait(lk, [this] {return pOutput != nullptr || dying;});
            if (dying)
            {
                break;
            }
            ProcessDataset(input, *pOutput);
            pOutput = nullptr;
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
    std::span<int> input;
    int* pOutput = nullptr;
};

std::vector<std::array<int, DATASET_SIZE>> GenerateDatasets()
{
    std::minstd_rand rne;
    std::vector<std::array<int, DATASET_SIZE>> datasets{ 4 };

    for (auto& set : datasets)
    {
        std::ranges::generate(set, rne);
    }

    return datasets;
}

int BigChunk()
{
    auto datasets = GenerateDatasets();

    std::vector<std::thread> workers;
    Timer timer;

    struct Number
    {
        int value = 0;
        char padding[60];
    };
    Number sum[4] = { 0 };

    timer.Mark();
    for (size_t i = 0; i < 4; i++)
    {
        workers.push_back(std::thread{ ProcessDataset, std::span{datasets[i]}, std::ref(sum[i].value) });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }
    const auto t = timer.Peek();

    std::cout << "Processing the datasets took " << t << " seconds" << std::endl;
    std::cout << "Result is " << (sum[0].value + sum[1].value + sum[2].value + sum[3].value) << std::endl;

    return 0;
}

int SmallChunk()
{
    auto datasets = GenerateDatasets();

    struct Number
    {
        int value = 0;
        char padding[60];
    };
    Number sum[4] = { 0 };

    Timer timer;
    timer.Mark();

    constexpr size_t workerCount = 4;
    MasterControl mctrl{ workerCount };
    std::vector<std::unique_ptr<Worker>> workerPtrs;
    for (size_t i = 0; i < workerCount; i++)
    {
        workerPtrs.push_back(std::make_unique<Worker>(&mctrl));
    }

    constexpr auto subsetSize = DATASET_SIZE / 10'000;
    for (size_t i = 0; i < DATASET_SIZE; i += subsetSize)
    {
        for (size_t j = 0; j < 4; j++)
        {
            workerPtrs[j]->SetJob(std::span{ &datasets[j][i], subsetSize }, &sum[j].value);
        }
        mctrl.WaitForAllDone();
    }
    const auto t = timer.Peek();

    std::cout << "Processing the datasets took " << t << " seconds" << std::endl;
    std::cout << "Result is " << (sum[0].value + sum[1].value + sum[2].value + sum[3].value) << std::endl;

    for (auto& worker : workerPtrs)
    {
        worker->Kill();
    }

    return 0;
}

int main()
{
    bool doSmall = true;
    if (doSmall)
    {
        return SmallChunk();
    }
    else
    {
        return BigChunk();
    }
}