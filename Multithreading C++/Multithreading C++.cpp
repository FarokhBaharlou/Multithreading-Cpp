
#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <ranges>
#include <cmath>
#include <limits>
#include <thread>
#include "Timer.h"

constexpr size_t DATASET_SIZE = 5000000;

void ProcessDataset(std::array<int, DATASET_SIZE>& set)
{
    for (int num : set)
    {
        constexpr auto limit = (double)std::numeric_limits<int>::max();
        const auto x = (double)num / limit;
        set[0] += int(std::sin(std::cos(x)) * limit);
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

    timer.Mark();
    for (auto& set : datasets)
    {
        workers.push_back(std::thread{ ProcessDataset, std::ref(set) });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }
    auto t = timer.Peek();

    std::cout << "Processing the datasets took " << t << " seconds" << std::endl;
    return 0;
}