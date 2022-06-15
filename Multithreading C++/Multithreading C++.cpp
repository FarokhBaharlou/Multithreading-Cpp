
#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <ranges>
#include <cmath>
#include <limits>
#include <thread>
#include <mutex>
#include "Timer.h"

constexpr size_t DATASET_SIZE = 5000000;

void ProcessDataset(std::array<int, DATASET_SIZE>& set, int& sum, std::mutex& mtx)
{
    for (int num : set)
    {
        //Locking and unlocking mutex has overhead which is not good for this situation of our large dataset
        std::lock_guard g{ mtx };
        constexpr auto limit = (double)std::numeric_limits<int>::max();
        const auto x = (double)num / limit;
        sum += int(std::sin(std::cos(x)) * limit);
    }
}

int main()
{
    std::minstd_rand rne;
    std::vector<std::array<int, DATASET_SIZE>> datasets{ 4 };
    std::vector<std::thread> workers;
    Timer timer;

    for (auto& set : datasets)
    {
        std::ranges::generate(set, rne);
    }

    std::mutex mtx;
    int sum = 0;

    timer.Mark();
    for (auto& set : datasets)
    {
        workers.push_back(std::thread{ ProcessDataset, std::ref(set), std::ref(sum), std::ref(mtx) });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }
    auto t = timer.Peek();

    std::cout << "Processing the datasets took " << t << " seconds" << std::endl;
    std::cout << "Result is " << sum << std::endl;
    return 0;
}