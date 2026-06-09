#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q41(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"WindowClientWidth", "WindowClientHeight", "CounterID", "EventDate", "IsRefresh",
                                      "DontCountHits", "URLHash"}));

    auto d1 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-31")});

    auto cmp = TransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "DontCountHits", "d"),
        ColumnOperation(
            Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{2868770270353813622LL})),
            "URLHash", "u"),
    });

    auto and1 = TransformOperator({ColumnOperation::LogicalAnd("c", "ge", "k1")});
    auto and2 = TransformOperator({ColumnOperation::LogicalAnd("k1", "le", "k2")});
    auto and3 = TransformOperator({ColumnOperation::LogicalAnd("k2", "r", "k3")});
    auto and4 = TransformOperator({ColumnOperation::LogicalAnd("k3", "d", "k4")});
    auto and5 = TransformOperator({ColumnOperation::LogicalAnd("k4", "u", "keep")});

    GroupByOperator group_by({"WindowClientWidth", "WindowClientHeight"},
                             {{.tp = AggType::Count, .inp_col = "WindowClientWidth", .out_col = "PageViews"}});

    size_t to_skip = 10000;
    size_t limit = 10;

    SortOperator sort1(
        {
            {.inp_col = "PageViews", .reversed = true},
        },
        limit + to_skip);

    SkipOperator skip(to_skip);

    return std::move(reader) >= cmp >= and1 >= and2 >= and3 >= and4 >= and5 >= FilterOperator("keep") >= group_by >=
           sort1 >= skip;
}

}  // namespace Q
