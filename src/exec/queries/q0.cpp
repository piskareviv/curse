#include <cstdlib>
#include <memory>
#include <string>

#include "../queries.hpp"
#include "src/core/operators.hpp"
#include "src/core/operators/count.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"
#include "src/exec/hits_schema.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q0(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {}));

    CountOperator count("1");

    return std::move(reader) >= count;
}

}  // namespace Q
