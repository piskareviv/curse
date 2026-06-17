#include "../queries.hpp"
#include "src/core/skippers.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT TraficSourceID, SearchEngineID, AdvEngineID, CASE WHEN (SearchEngineID = 0 AND AdvEngineID = 0) THEN Referer
// ELSE '' END AS Src, URL AS Dst, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01'
// AND EventDate <= '2013-07-31' AND IsRefresh = 0 GROUP BY TraficSourceID, SearchEngineID, AdvEngineID, Src, Dst ORDER
// BY PageViews DESC LIMIT 10 OFFSET 1000;
std::unique_ptr<BatchStream> Q39(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"TraficSourceID", "SearchEngineID", "AdvEngineID", "Referer", "URL", "CounterID",
                                      "EventDate", "IsRefresh"}));

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

    BasicSkipper skip_filter({sel1, sel2, sel3, sel4});

    auto projection = TransformOperator(
        {ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                         "SearchEngineID", "se0"),
         ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                         "AdvEngineID", "ae0"),
         ColumnOperation::LogicalAnd("se0", "ae0", "src_cond"),
         ColumnOperation(Transform::Constant(Value::From<TypeId::String>("")), "Referer", "empty"),
         ColumnOperation::Select("src_cond", "Referer", "empty", "Src")});

    GroupByOperator group_by({"TraficSourceID", "SearchEngineID", "AdvEngineID", "Src", "URL"},
                             {{.tp = AggType::Count, .inp_col = "URL", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}}, 1000 + 10);
    SkipOperator skip_offset(1000);
    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= skip_filter >= projection >= group_by >= sort1 >= skip_offset >= sort2;
}

}  // namespace Q
