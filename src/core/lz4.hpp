#pragma once

#include <span>
#include <vector>

namespace curse {

void CompressLZ4(const std::span<const char> span, std::vector<char> &vec);
void DecompressLZ4(const std::span<const char> span, std::vector<char> &vec);

}  // namespace curse
