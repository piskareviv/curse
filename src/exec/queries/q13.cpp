#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q13(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "UserID"}));
    auto filter = FilterOperator("SearchPhrase");
    auto group_by = GroupByOperator(
        {"SearchPhrase"}, {GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});
    auto sort = SortOperator({{.inp_col = "u", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

}  // namespace Q
