#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q42(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(
        file, SubSchema(kHitsSchema, {"EventTime", "EventDate", "CounterID", "IsRefresh", "DontCountHits"}));

    auto d1 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-14")});

    auto d2 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-15")});

    auto trs = TransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "DontCountHits", "d"),
        //
        ColumnOperation::LogicalAnd("c", "ge", "k1"),
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
        ColumnOperation::LogicalAnd("k2", "r", "k3"),
        ColumnOperation::LogicalAnd("k3", "d", "keep"),
    });

    auto trunc = TransformOperator({ColumnOperation(Transform::TruncateToMinutes(), "EventTime", "M")});

    FilterOperator keep("keep");

    GroupByOperator group_by({"M"}, {{.tp = AggType::Count, .inp_col = "EventTime", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "M"}});

    SkipOperator skip(1000);

    SortOperator sort2({{.inp_col = "M"}}, 10);

    return std::move(reader) >= trs >= keep >= trunc >= group_by >= sort1 >= skip >= sort2;
}

}  // namespace Q
