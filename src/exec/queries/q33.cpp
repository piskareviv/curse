#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q33(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"URL"}));

    GroupByOperator group_by({"URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "c"}});

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

}  // namespace Q
