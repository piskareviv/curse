#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT SearchEngineID, SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP BY SearchEngineID, SearchPhrase ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q14(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"SearchEngineID", "SearchPhrase"}));
    auto filter = FilterOperator("SearchPhrase");
    auto group_by =
        GroupByOperator({"SearchEngineID", "SearchPhrase"},
                        {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "SearchPhrase", .out_col = "c"}});
    auto sort = SortOperator({{.inp_col = "c", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

}  // namespace Q
