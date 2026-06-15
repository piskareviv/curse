#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q19(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    auto cmp = TransformOperator({ColumnOperation(
        Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{435090932899640449LL})),
        "UserID", "match")});

    FilterOperator filter("match");

    return std::move(reader) >= cmp >= filter >= DropOperator({"match"});
}

}  // namespace Q
