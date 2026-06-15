#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q23(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, kHitsSchema);

    auto trs = TransformOperator({ColumnOperation(Transform::RegexpSearch("google"), "URL", "match")});

    FilterOperator filter("match");

    SortOperator sort({{.inp_col = "EventTime"}}, 10);

    return std::move(reader) >= trs >= filter >= DropOperator({"match"}) >= sort;
}

}  // namespace Q
