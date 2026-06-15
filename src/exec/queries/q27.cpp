#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q27(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"CounterID", "URL"}));

    FilterOperator non_empty("URL");

    auto trs = TransformOperator({ColumnOperation(Transform::Strlen(), "URL", "len")});

    GroupByOperator group_by({"CounterID"}, {
                                                {.tp = AggType::Average, .inp_col = "len", .out_col = "l"},
                                                {.tp = AggType::Count, .inp_col = "CounterID", .out_col = "c"},
                                            });

    auto having = TransformOperator({ColumnOperation(
        Transform::Compare(Transform::ComparisonType::GreaterThan, Value(ValueT<TypeId::Int64>{100000})), "c",
        "keep")});

    FilterOperator keep("keep");

    SortOperator sort({{.inp_col = "l", .reversed = true}}, 25);

    auto drop = DropOperator({"keep"});

    return std::move(reader) >= non_empty >= trs >= group_by >= having >= keep >= drop >= sort;
}

}  // namespace Q
