#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT Title, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31' AND DontCountHits = 0 AND IsRefresh = 0 AND Title <> '' GROUP BY Title ORDER BY PageViews DESC LIMIT 10;
std::unique_ptr<BatchStream> Q37(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(
        file, SubSchema(kHitsSchema, {"Title", "CounterID", "EventDate", "DontCountHits", "IsRefresh"}));

    auto d1 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-31")});

    auto filt = TransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "DontCountHits", "d"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),

        ColumnOperation::LogicalAnd("c", "ge", "k1"),
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
        ColumnOperation::LogicalAnd("k2", "d", "k3"),
        ColumnOperation::LogicalAnd("k3", "r", "keep"),
    });

    FilterOperator keep("keep");
    FilterOperator title("Title");

    GroupByOperator group_by({"Title"}, {{.tp = AggType::Count, .inp_col = "Title", .out_col = "PageViews"}});

    SortOperator sort({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= filt >= keep >= title >= group_by >= sort;
}

}  // namespace Q
