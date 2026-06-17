#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT SearchPhrase, MIN(URL), COUNT(*) AS c FROM hits WHERE URL LIKE '%google%' AND SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q21(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "URL"}));

    auto trs = TransformOperator({ColumnOperation(Transform::RegexpSearch("google"), "URL", "google")});

    FilterOperator google("google");
    FilterOperator phrase("SearchPhrase");

    GroupByOperator group_by({"SearchPhrase"}, {
                                                   {.tp = AggType::Min, .inp_col = "URL", .out_col = "url"},
                                                   {.tp = AggType::Count, .inp_col = "URL", .out_col = "c"},
                                               });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= trs >= google >= phrase >= group_by >= sort;
}

}  // namespace Q
