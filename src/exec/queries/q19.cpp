#include "../queries.hpp"
#include "src/core/skippers.hpp"
#include "src/core/storage.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT UserID FROM hits WHERE UserID = 435090932899640449;
std::unique_ptr<BatchStream> Q19(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    auto skip = BasicSkipper({std::make_shared<TransformSelector>(
        "UserID",
        Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{435090932899640449LL})))});

    return std::move(reader) >= skip;
}

}  // namespace Q
