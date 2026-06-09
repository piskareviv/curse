#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q29(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"ResolutionWidth"}));

    AggregationOperator agg({
        {.tp = AggType::Sum, .inp_col = "ResolutionWidth", .out_col = "sum0"},
        {.tp = AggType::Count, .inp_col = "ResolutionWidth", .out_col = "cnt"},
    });

    std::vector<ColumnOperation> ops;
    std::vector<std::string> result_cols = {"sum0"};

    for (int i = 1; i < 90; ++i) {
        const auto c = std::format("c{}", i);
        const auto mul = std::format("mul{}", i);
        const auto out = std::format("sum{}", i);

        ops.emplace_back(Transform::Constant(Value(ValueT<TypeId::Int64>{i})), "cnt", c);

        ops.emplace_back(ColumnOperation::ArithmeticOp("cnt", c, mul, ColumnOperation::Arithmetic::Mul, TypeId::Int64));

        ops.emplace_back(
            ColumnOperation::ArithmeticOp("sum0", mul, out, ColumnOperation::Arithmetic::Add, TypeId::Int64));

        result_cols.push_back(out);
    }

    auto trs = TransformOperator(std::move(ops));
    auto select = SelectOperator(std::move(result_cols));

    return std::move(reader) >= agg >= trs >= select;
}

}  // namespace Q
