#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q5(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));

    AggregationOperator count_distinct({
        AggregationOperator::Params{.tp = AggType::CountDistinct, .inp_col = "SearchPhrase", .out_col = "1"},
    });

    return std::move(reader) >= count_distinct;
}

}  // namespace Q
