#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q24(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "EventTime"}));
    auto filter = FilterOperator("SearchPhrase");
    auto sort = SortOperator({{.inp_col = "EventTime"}}, 10);
    return std::move(reader) >= filter >= sort;
}

}  // namespace Q
