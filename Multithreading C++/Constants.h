#pragma once

inline constexpr bool chunkMeasurementEnabled = false;
inline constexpr size_t workerCount = 4;
inline constexpr size_t chunkSize = 8'000;
inline constexpr size_t chunkCount = 100;
inline constexpr size_t lightIterations = 100;
inline constexpr size_t heavyIterations = 1'000;
inline constexpr double probabilityHeavy = 0.15;

inline constexpr size_t subsetSize = chunkSize / workerCount;

static_assert(chunkSize >= workerCount);
static_assert(chunkSize % workerCount == 0);