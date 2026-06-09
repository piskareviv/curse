#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q12(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));
    auto filter = FilterOperator("SearchPhrase");
    auto group_by = GroupByOperator(
        {"SearchPhrase"}, {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "SearchPhrase", .out_col = "c"}});
    auto sort = SortOperator({{.inp_col = "c", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

}  // namespace Q
