#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT RegionID, COUNT(DISTINCT UserID) AS u FROM hits GROUP BY RegionID ORDER BY u DESC LIMIT 10;
std::unique_ptr<BatchStream> Q8(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"RegionID", "UserID"}));

    auto group_by = GroupByOperator(
        {"RegionID"}, {GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});

    auto sort = SortOperator({{.inp_col = "u", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

}  // namespace Q
