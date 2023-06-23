#pragma once

#include <array>
#include <span>
#include <fstream>
#include <format>
#include "Constants.h"

struct ChunkTimingInfo
{
    std::array<float, workerCount> timeSpentWorkingPerThread;
    std::array<size_t, workerCount> numberOfHeavyItemsPerThread;
    float totalChunkTime;
};

void WriteCSV(const std::span<const ChunkTimingInfo> timings)
{
    std::ofstream csv{ "timings.csv", std::ios_base::trunc };
    for (size_t i = 0; i < workerCount; i++)
    {
        csv << std::format("work_{0:},idle_{0:},heavy_{0:},", i);
    }
    csv << "chunktime,total_idle,total_heavy\n";

    for (const auto& chunk : timings)
    {
        float totalIdle = 0.f;
        size_t totalHeavy = 0;
        for (size_t i = 0; i < workerCount; i++)
        {
            const auto idle = chunk.totalChunkTime - chunk.timeSpentWorkingPerThread[i];
            const auto heavy = chunk.numberOfHeavyItemsPerThread[i];
            csv << std::format("{},{},{},", chunk.timeSpentWorkingPerThread[i], idle, heavy);
            totalIdle += idle;
            totalHeavy += heavy;
        }
        csv << std::format("{},{},{}\n", chunk.totalChunkTime, totalIdle, totalHeavy);
    }
}