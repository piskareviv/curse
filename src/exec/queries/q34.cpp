#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q34(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"URL"}));

    auto trs = TransformOperator({ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{1})), "URL", "one")});

    GroupByOperator group_by({"one", "URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "c"}});

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= trs >= group_by >= sort;
}

}  // namespace Q
