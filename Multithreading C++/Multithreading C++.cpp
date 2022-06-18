
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

    int total = 0;
    std::vector<std::jthread> workers;
    constexpr auto subsetSize = DATASET_SIZE / 10'000;
    for (size_t i = 0; i < DATASET_SIZE; i += subsetSize)
    {
        for (size_t j = 0; j < 4; j++)
        {
            workers.push_back(std::jthread{ ProcessDataset, std::span{&datasets[j][i], subsetSize}, std::ref(sum[j].value) });
        }
        workers.clear();
        total = sum[0].value + sum[1].value + sum[2].value + sum[3].value;
    }
    const auto t = timer.Peek();

    std::cout << "Processing the datasets took " << t << " seconds" << std::endl;
    std::cout << "Result is " << (sum[0].value + sum[1].value + sum[2].value + sum[3].value) << std::endl;

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