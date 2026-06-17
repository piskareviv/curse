#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT COUNT(DISTINCT UserID) FROM hits;
std::unique_ptr<BatchStream> Q4(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    AggregationOperator count_distinct({
        AggregationOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "1"},
    });

    return std::move(reader) >= count_distinct;
}

}  // namespace Q
