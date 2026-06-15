#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q15(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));
    auto group_by = GroupByOperator(
        {"UserID"}, {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "cnt"}});
    auto sort = SortOperator({{.inp_col = "cnt", .reversed = true}}, 10);
    return std::move(reader) >= group_by >= sort;
}

}  // namespace Q
