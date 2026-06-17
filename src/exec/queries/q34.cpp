#include "../queries.hpp"
#include "src/core/operators/select.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT 1, URL, COUNT(*) AS c FROM hits GROUP BY 1, URL ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q34(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"URL"}));
    auto trs = TransformOperator({ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{1})), "URL", "one")});
    GroupByOperator group_by({"URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "c"}});
    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);
    SelectOperator select({"one", "URL"});

    return std::move(reader) >= group_by >= sort >= trs >= select;
}

}  // namespace Q
