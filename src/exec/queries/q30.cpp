#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT SearchEngineID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits WHERE SearchPhrase <> '' GROUP BY SearchEngineID, ClientIP ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q30(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(
        file, SubSchema(kHitsSchema, {"SearchEngineID", "ClientIP", "IsRefresh", "ResolutionWidth", "SearchPhrase"}));

    FilterOperator phrase("SearchPhrase");

    GroupByOperator group_by({"SearchEngineID", "ClientIP"},
                             {
                                 {.tp = AggType::Count, .inp_col = "ClientIP", .out_col = "c"},
                                 {.tp = AggType::Sum, .inp_col = "IsRefresh", .out_col = "r"},
                                 {.tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "w"},
                             });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= phrase >= group_by >= sort;
}

}  // namespace Q
