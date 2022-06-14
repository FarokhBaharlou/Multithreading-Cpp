
#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <ranges>
#include "Timer.h"

constexpr size_t DATASET_SIZE = 5000000;

int main()
{
    std::minstd_rand rne;
    std::vector<std::array<int, DATASET_SIZE>> datasets{ 4 };
    Timer timer;

    timer.Mark();
    for (auto& arr : datasets)
    {
        std::ranges::generate(arr, rne);
    }
    auto t = timer.Peek();

    std::cout << "Generating the datasets took " << t << " seconds" << std::endl;
    return 0;
}