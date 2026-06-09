#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q40(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file,
        SubSchema(kHitsSchema, {"URLHash", "EventDate", "CounterID", "IsRefresh", "TraficSourceID", "RefererHash"}));

    auto d1 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-31")});

    auto trs = TransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{-1})),
                        "TraficSourceID", "ts_neg1"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{6})),
                        "TraficSourceID", "ts_6"),
        ColumnOperation(
            Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{3594120000172545465LL})),
            "RefererHash", "rh"),

        ColumnOperation::LogicalOr("ts_neg1", "ts_6", "ts_ok"),
        ColumnOperation::LogicalAnd("c", "ge", "k1"),
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
        ColumnOperation::LogicalAnd("k2", "r", "k3"),
        ColumnOperation::LogicalAnd("k3", "rh", "k4"),
        ColumnOperation::LogicalAnd("k4", "ts_ok", "keep"),
    });

    GroupByOperator({"URLHash", "EventDate"}, {{.tp = AggType::Count, .inp_col = "URLHash", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}});

    SkipOperator skip(100);

    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= trs >= FilterOperator("keep") >=
           GroupByOperator({"URLHash", "EventDate"},
                           {{.tp = AggType::Count, .inp_col = "URLHash", .out_col = "PageViews"}}) >= sort1 >= skip >=
           sort2;
}

}  // namespace Q
