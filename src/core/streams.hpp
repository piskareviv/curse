#pragma once

#include <memory>

#include "src/core/types.hpp"

namespace curse {

std::unique_ptr<BatchStream> MakeSingletonStream(std::unique_ptr<Batch> batch);
std::unique_ptr<Batch> ReadAll(std::unique_ptr<BatchStream> stream);

}  // namespace curse
