#include "../queries.hpp"
#include "src/core/skippers.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT DATE_TRUNC('minute', EventTime) AS M, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >=
// '2013-07-14' AND EventDate <= '2013-07-15' AND IsRefresh = 0 AND DontCountHits = 0 GROUP BY DATE_TRUNC('minute',
// EventTime) ORDER BY DATE_TRUNC('minute', EventTime) LIMIT 10 OFFSET 1000;
std::unique_ptr<BatchStream> Q42(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"EventTime", "EventDate", "CounterID", "IsRefresh", "DontCountHits"}));

    auto d1 = Value::From<TypeId::Date>(ConvertVal<TypeId::Date>::FromString("2013-07-14"));
    auto d2 = Value::From<TypeId::Date>(ConvertVal<TypeId::Date>::FromString("2013-07-15"));

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

    BasicSkipper early_skip({sel1, sel2, sel3, sel4, sel5});

    auto trunc = TransformOperator({ColumnOperation("EventTime", "M", Transform::TruncateToMinutes())});
    GroupByOperator group_by({"M"}, {{.tp = AggType::Count, .inp_col = "EventTime", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "M"}}, 1000 + 10);
    SkipOperator skip_offset(1000);
    SortOperator sort2({{.inp_col = "M"}}, 10);

    return std::move(reader) >= early_skip >= trunc >= group_by >= sort1 >= skip_offset >= sort2;
}

}  // namespace Q
