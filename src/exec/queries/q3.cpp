#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q3(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    AggregationOperator avg({
        AggregationOperator::Params{.tp = AggType::Average, .inp_col = "UserID", .out_col = "1"},
    });

    return std::move(reader) >= avg;
}

}  // namespace Q
