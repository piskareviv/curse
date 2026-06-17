#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT REGEXP_REPLACE(Referer, '^https?://(?:www\.)?([^/]+)/.*$', '\1') AS k, AVG(length(Referer)) AS l, COUNT(*) AS
// c, MIN(Referer) FROM hits WHERE Referer <> '' GROUP BY k HAVING COUNT(*) > 100000 ORDER BY l DESC LIMIT 25;
std::unique_ptr<BatchStream> Q28(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"Referer"}));

    FilterOperator non_empty("Referer");

    auto trs = TransformOperator(
        {ColumnOperation(Transform::RegexpReplace("^https?://(?:www\\.)?([^/]+)/.*$", "\\1"), "Referer", "k"),
         ColumnOperation(Transform::Strlen(), "Referer", "len")});

    GroupByOperator group_by({"k"}, {
                                        {.tp = AggType::Average, .inp_col = "len", .out_col = "l"},
                                        {.tp = AggType::Count, .inp_col = "Referer", .out_col = "c"},
                                        {.tp = AggType::Min, .inp_col = "Referer", .out_col = "r"},
                                    });

    auto having = TransformOperator({ColumnOperation(
        Transform::Compare(Transform::ComparisonType::GreaterThan, Value(ValueT<TypeId::Int64>{100000})), "c",
        "keep")});

    FilterOperator keep("keep");

    SortOperator sort({{.inp_col = "l", .reversed = true}}, 25);

    SelectOperator select({"k", "l", "c", "r"});

    return std::move(reader) >= non_empty >= trs >= group_by >= having >= keep >= select >= sort;
}

}  // namespace Q
