#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT UserID, extract(minute FROM EventTime) AS m, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, m, SearchPhrase ORDER BY COUNT(*) DESC LIMIT 10;
std::unique_ptr<BatchStream> Q18(const std::string& file) {
    auto reader =
        std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"UserID", "EventTime", "SearchPhrase"}));

    auto minute_op = ColumnOperation(Transform::ExtractMinute(), "EventTime", "m");
    auto transform = TransformOperator({minute_op});

    auto group_by =
        GroupByOperator({"UserID", "m", "SearchPhrase"},
                        {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "cnt"}});

    auto sort = SortOperator({{.inp_col = "cnt", .reversed = true}}, 10);
    return std::move(reader) >= transform >= group_by >= sort;
}

}  // namespace Q
