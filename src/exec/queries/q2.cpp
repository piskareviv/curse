#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT SUM(AdvEngineID), COUNT(*), AVG(ResolutionWidth) FROM hits;
std::unique_ptr<BatchStream> Q2(const std::string& file) {
    std::unique_ptr<BatchStream> reader =
        std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID", "ResolutionWidth"}));

    AggregationOperator aggr({
        AggregationOperator::Params{.tp = AggType::Sum, .inp_col = "AdvEngineID", .out_col = "1"},
        AggregationOperator::Params{.tp = AggType::Count, .inp_col = "AdvEngineID", .out_col = "2"},
        AggregationOperator::Params{.tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "3"},
    });

    return std::move(reader) >= aggr;
}

}  // namespace Q
