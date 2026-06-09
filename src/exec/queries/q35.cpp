#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q35(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"ClientIP"}));

    auto group_by = GroupByOperator({"ClientIP"}, {
                                                      {.tp = AggType::Count, .inp_col = "ClientIP", .out_col = "c"},
                                                  });

    auto sort = SortOperator({{.inp_col = "c", .reversed = true}}, 10);

    auto trs = TransformOperator({
        ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{1})), "ClientIP", "one"),
        ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{2})), "ClientIP", "two"),
        ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{3})), "ClientIP", "three"),

        ColumnOperation::ArithmeticOp("ClientIP", "one", "ClientIP_m1", ColumnOperation::Arithmetic::Sub,
                                      TypeId::Int32),
        ColumnOperation::ArithmeticOp("ClientIP", "two", "ClientIP_m2", ColumnOperation::Arithmetic::Sub,
                                      TypeId::Int32),
        ColumnOperation::ArithmeticOp("ClientIP", "three", "ClientIP_m3", ColumnOperation::Arithmetic::Sub,
                                      TypeId::Int32),
    });

    auto select = SelectOperator({
        "ClientIP",
        "ClientIP_m1",
        "ClientIP_m2",
        "ClientIP_m3",
        "c",
    });

    return std::move(reader) >= group_by >= sort >= trs >= select;
}

}  // namespace Q
