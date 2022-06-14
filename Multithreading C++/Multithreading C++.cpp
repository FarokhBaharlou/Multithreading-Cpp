
#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <ranges>
#include <cmath>
#include <limits>
#include "Timer.h"

constexpr size_t DATASET_SIZE = 5000000;

int main()
{
    std::minstd_rand rne;
    std::vector<std::array<int, DATASET_SIZE>> datasets{ 4 };
    Timer timer;

    for (auto& set : datasets)
    {
        std::ranges::generate(set, rne);
    }

    timer.Mark();
    for (auto& set : datasets)
    {
        for (int num : set)
        {
            constexpr auto limit = (double)std::numeric_limits<int>::max();
            const auto x = (double)num / limit;
            set[0] += int(std::sin(std::cos(x)) * limit);
        }
    }
    auto t = timer.Peek();

    std::cout << "Processing the datasets took " << t << " seconds" << std::endl;
    return 0;
}