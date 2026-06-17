#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase ORDER BY COUNT(*) DESC LIMIT 10;
std::unique_ptr<BatchStream> Q16(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"UserID", "SearchPhrase"}));
    auto group_by =
        GroupByOperator({"UserID", "SearchPhrase"},
                        {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "cnt"}});
    auto sort = SortOperator({{.inp_col = "cnt", .reversed = true}}, 10);
    return std::move(reader) >= group_by >= sort;
}

}  // namespace Q
