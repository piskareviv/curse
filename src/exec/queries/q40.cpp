#include "../queries.hpp"
#include "src/core/operators/filter.hpp"
#include "src/core/skippers.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT URLHash, EventDate, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND
// EventDate <= '2013-07-31' AND IsRefresh = 0 AND TraficSourceID IN (-1, 6) AND RefererHash = 3594120000172545465 GROUP
// BY URLHash, EventDate ORDER BY PageViews DESC LIMIT 10 OFFSET 100;
std::unique_ptr<BatchStream> Q40(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file,
        SubSchema(kHitsSchema, {"URLHash", "EventDate", "CounterID", "IsRefresh", "TraficSourceID", "RefererHash"}));

    auto d1 = Value::From<TypeId::Date>(ConvertVal<TypeId::Date>::FromString("2013-07-01"));
    auto d2 = Value::From<TypeId::Date>(ConvertVal<TypeId::Date>::FromString("2013-07-31"));

    auto sel1 = std::make_shared<TransformSelector>(
        "CounterID", Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})));
    auto sel2 = std::make_shared<TransformSelector>(
        "EventDate", Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1));
    auto sel3 = std::make_shared<TransformSelector>("EventDate",
                                                    Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2));
    auto sel4 = std::make_shared<TransformSelector>(
        "IsRefresh", Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})));
    auto sel5 = std::make_shared<TransformSelector>(
        "RefererHash",
        Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{3594120000172545465LL})));

    BasicSkipper early_skip({sel5, sel1, sel2, sel3, sel4});

    auto ts_check = TransformOperator(
        {ColumnOperation("TraficSourceID", "ts_neg1",
                         Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{-1}))),
         ColumnOperation("TraficSourceID", "ts_6",
                         Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{6}))),
         ColumnOperation::LogicalOr("ts_neg1", "ts_6", "ts_ok")});
    FilterOperator filt("ts_ok");

    GroupByOperator group_by({"URLHash", "EventDate"},
                             {{.tp = AggType::Count, .inp_col = "URLHash", .out_col = "PageViews"}});
    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}}, 100 + 10);
    SkipOperator skip_offset(100);
    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= early_skip >= ts_check >= filt >= group_by >= sort1 >= skip_offset >= sort2;
}

}  // namespace Q
