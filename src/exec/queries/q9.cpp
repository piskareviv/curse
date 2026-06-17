#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT RegionID, SUM(AdvEngineID), COUNT(*) AS c, AVG(ResolutionWidth), COUNT(DISTINCT UserID) FROM hits GROUP BY
// RegionID ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q9(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(
        file, SubSchema(kHitsSchema, {"RegionID", "AdvEngineID", "ResolutionWidth", "UserID"}));

    auto group_by = GroupByOperator(
        {"RegionID"},
        {
            GroupByOperator::Params{.tp = AggType::Sum, .inp_col = "AdvEngineID", .out_col = "sum_AdvEngineID"},
            GroupByOperator::Params{.tp = AggType::Count, .inp_col = "RegionID", .out_col = "c"},
            GroupByOperator::Params{
                .tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "avg_ResolutionWidth"},
            GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "distinct_UserID"},
        });

    auto sort = SortOperator({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

}  // namespace Q
