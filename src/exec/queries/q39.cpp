#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q39(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"TraficSourceID", "SearchEngineID", "AdvEngineID", "Referer", "URL", "CounterID",
                                      "EventDate", "IsRefresh"}));

    auto d1 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{ConvertVal<TypeId::Date>::FromString("2013-07-31")});

    auto trs = TransformOperator(
        {ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                         "CounterID", "c"),
         ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),
         ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),
         ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                         "IsRefresh", "r"),
         ColumnOperation::LogicalAnd("c", "ge", "k1"), ColumnOperation::LogicalAnd("k1", "le", "k2"),
         ColumnOperation::LogicalAnd("k2", "r", "keep"),
         ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                         "SearchEngineID", "se0"),
         ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                         "AdvEngineID", "ae0"),
         ColumnOperation::LogicalAnd("se0", "ae0", "src_cond"),
         ColumnOperation(Transform::Constant(Value(ValueT<TypeId::String>{std::string_view("")})), "Referer", "empty"),
         ColumnOperation::Select("src_cond", "Referer", "empty", "Src")});

    GroupByOperator group_by({"TraficSourceID", "SearchEngineID", "AdvEngineID", "Src", "URL"},
                             {{.tp = AggType::Count, .inp_col = "URL", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}}, 1000 + 10);

    SkipOperator skip(1000);

    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= trs >= FilterOperator("keep") >= group_by >= sort1 >= skip >= sort2;
}

}  // namespace Q
