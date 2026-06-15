#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q31(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(
        file, SubSchema(kHitsSchema, {"WatchID", "ClientIP", "IsRefresh", "ResolutionWidth", "SearchPhrase"}));

    FilterOperator phrase("SearchPhrase");

    GroupByOperator group_by({"WatchID", "ClientIP"},
                             {
                                 {.tp = AggType::Count, .inp_col = "WatchID", .out_col = "c"},
                                 {.tp = AggType::Sum, .inp_col = "IsRefresh", .out_col = "r"},
                                 {.tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "w"},
                             });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= phrase >= group_by >= sort;
}

}  // namespace Q
