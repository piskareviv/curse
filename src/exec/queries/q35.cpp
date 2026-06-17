#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT ClientIP, ClientIP - 1, ClientIP - 2, ClientIP - 3, COUNT(*) AS c FROM hits GROUP BY ClientIP, ClientIP - 1,
// ClientIP - 2, ClientIP - 3 ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q35(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"ClientIP"}));

    auto group_by = GroupByOperator({"ClientIP"}, {{.tp = AggType::Count, .inp_col = "ClientIP", .out_col = "c"}});
    auto sort = SortOperator({{.inp_col = "c", .reversed = true}}, 10);

    auto trs = TransformOperator({
        ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{1})), "ClientIP", "_1"),
        ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{2})), "ClientIP", "_2"),
        ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{3})), "ClientIP", "_3"),

        ColumnOperation::ArithmeticOp("ClientIP", "_1", "ip1", ColumnOperation::Arithmetic::Sub, TypeId::Int32),
        ColumnOperation::ArithmeticOp("ClientIP", "_2", "ip2", ColumnOperation::Arithmetic::Sub, TypeId::Int32),
        ColumnOperation::ArithmeticOp("ClientIP", "_3", "ip3", ColumnOperation::Arithmetic::Sub, TypeId::Int32),
    });

    auto select = SelectOperator({
        "ClientIP",
        "ip1",
        "ip2",
        "ip3",
        "c",
    });

    return std::move(reader) >= group_by >= sort >= trs >= select;
}

}  // namespace Q
