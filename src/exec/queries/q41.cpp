#include "../queries.hpp"
#include "src/core/skippers.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT WindowClientWidth, WindowClientHeight, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >=
// '2013-07-01' AND EventDate <= '2013-07-31' AND IsRefresh = 0 AND DontCountHits = 0 AND URLHash = 2868770270353813622
// GROUP BY WindowClientWidth, WindowClientHeight ORDER BY PageViews DESC LIMIT 10 OFFSET 10000;
std::unique_ptr<BatchStream> Q41(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"WindowClientWidth", "WindowClientHeight", "CounterID", "EventDate", "IsRefresh",
                                      "DontCountHits", "URLHash"}));

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
        "DontCountHits", Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})));
    auto sel6 = std::make_shared<TransformSelector>(
        "URLHash",
        Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{2868770270353813622LL})));

    BasicSkipper early_skip({sel6, sel1, sel2, sel3, sel4, sel5});

    GroupByOperator group_by({"WindowClientWidth", "WindowClientHeight"},
                             {{.tp = AggType::Count, .inp_col = "WindowClientWidth", .out_col = "PageViews"}});

    size_t to_skip = 10000;
    size_t limit = 10;

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}}, limit + to_skip);

    SkipOperator skip_offset(to_skip);

    return std::move(reader) >= early_skip >= group_by >= sort1 >= skip_offset;
}

}  // namespace Q
