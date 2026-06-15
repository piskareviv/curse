#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q32(const std::string& file) {
    auto reader1 = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"WatchID", "ClientIP"}));
    auto reader2 = std::make_unique<SimpleCurseReader>(
        file, SubSchema(kHitsSchema, {"WatchID", "ClientIP", "IsRefresh", "ResolutionWidth"}));

    GroupByOperator group_by({"WatchID", "ClientIP"}, {{.tp = AggType::Count, .inp_col = "WatchID", .out_col = "c"}});
    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    std::unique_ptr<BatchStream> stream =
        std::move(reader1) >= group_by >= sort >= SelectOperator({"WatchID", "ClientIP"});

    auto trs = TransformOperator({ColumnOperation::SetContains({"WatchID", "ClientIP"}, "keep", std::move(stream))});

    GroupByOperator group_by2({"WatchID", "ClientIP"},
                              {
                                  {.tp = AggType::Count, .inp_col = "WatchID", .out_col = "c"},
                                  {.tp = AggType::Sum, .inp_col = "IsRefresh", .out_col = "r"},
                                  {.tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "w"},
                              });

    return std::move(reader2) >= trs >= FilterOperator("keep") >= group_by2 >= sort;
}

}  // namespace Q
