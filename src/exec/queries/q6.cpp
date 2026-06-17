#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT MIN(EventDate), MAX(EventDate) FROM hits;
std::unique_ptr<BatchStream> Q6(const std::string& file) {
    std::unique_ptr<BatchStream> reader =
        std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"EventDate"}));

    AggregationOperator min_max({
        AggregationOperator::Params{.tp = AggType::Min, .inp_col = "EventDate", .out_col = "1"},
        AggregationOperator::Params{.tp = AggType::Max, .inp_col = "EventDate", .out_col = "2"},
    });

    return std::move(reader) >= min_max;
}

}  // namespace Q
