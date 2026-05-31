#pragma once
#include <array>
#include <cstddef>

// approximate target batch memory consumption in bytes
constexpr size_t kBatchMemory = 128 * (1 << 20);

constexpr std::array<char, 4> kFormatMarker = {'c', 'r', 's', 'd'};
