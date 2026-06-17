#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT COUNT(*) FROM hits WHERE URL LIKE '%google%';
std::unique_ptr<BatchStream> Q20(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"URL"}));

    auto trs = TransformOperator({ColumnOperation(Transform::RegexpSearch("google"), "URL", "match")});

    FilterOperator filter("match");

    AggregationOperator count({{.tp = AggType::Count, .inp_col = "URL", .out_col = "1"}});

    return std::move(reader) >= trs >= filter >= count;
}

}  // namespace Q
