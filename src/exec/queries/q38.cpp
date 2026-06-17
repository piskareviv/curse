#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT URL, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31' AND IsRefresh = 0 AND IsLink <> 0 AND IsDownload = 0 GROUP BY URL ORDER BY PageViews DESC LIMIT 10 OFFSET 1000;
std::unique_ptr<BatchStream> Q38(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(
        file, SubSchema(kHitsSchema, {"URL", "CounterID", "EventDate", "IsRefresh", "IsLink", "IsDownload"}));

    auto d1 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-01")});
    auto d2 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-31")});

    auto filt = TransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::NotEqual, Value(ValueT<TypeId::Int16>{0})),
                        "IsLink", "l"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsDownload", "d"),

        ColumnOperation::LogicalAnd("c", "ge", "k1"),
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
        ColumnOperation::LogicalAnd("k2", "r", "k3"),
        ColumnOperation::LogicalAnd("k3", "l", "k4"),
        ColumnOperation::LogicalAnd("k4", "d", "keep"),
    });

    GroupByOperator group_by({"URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}}, 1000 + 10);

    SkipOperator skip(1000);

    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= filt >= FilterOperator("keep") >= group_by >= sort1 >= skip >= sort2;
}

}  // namespace Q
