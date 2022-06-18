
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

void ProcessDataset(std::array<int, DATASET_SIZE>& set, int& sum)
{
    for (int num : set)
    {
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

    struct Number
    {
        int value = 0;
        char padding[60];
    };
    Number sum[4] = { 0 };

    timer.Mark();
    for (size_t i = 0; i < 4; i++)
    {
        workers.push_back(std::thread{ ProcessDataset, std::ref(datasets[i]), std::ref(sum[i].value) });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }
    auto t = timer.Peek();

    std::cout << "Processing the datasets took " << t << " seconds" << std::endl;
    std::cout << "Result is " << (sum[0].value + sum[1].value + sum[2].value + sum[3].value) << std::endl;
    return 0;
}