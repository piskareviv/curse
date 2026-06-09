#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q25(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));
    auto filter = FilterOperator("SearchPhrase");
    auto sort = SortOperator({{.inp_col = "SearchPhrase"}}, 10);
    return std::move(reader) >= filter >= sort;
}

}  // namespace Q
