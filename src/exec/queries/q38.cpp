#include "../queries.hpp"
#include "src/core/skippers.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT URL, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <=
// '2013-07-31' AND IsRefresh = 0 AND IsLink <> 0 AND IsDownload = 0 GROUP BY URL ORDER BY PageViews DESC LIMIT 10
// OFFSET 1000;
std::unique_ptr<BatchStream> Q38(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"URL", "CounterID", "EventDate", "IsRefresh", "IsLink", "IsDownload"}));

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
        "IsLink", Transform::Compare(Transform::ComparisonType::NotEqual, Value(ValueT<TypeId::Int16>{0})));
    auto sel6 = std::make_shared<TransformSelector>(
        "IsDownload", Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})));

    BasicSkipper skip({sel1, sel2, sel3, sel4, sel5, sel6});

    GroupByOperator group_by({"URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}}, 1000 + 10);

    SkipOperator skip_op(1000);

    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= skip >= group_by >= sort1 >= skip_op >= sort2;
}

}  // namespace Q
