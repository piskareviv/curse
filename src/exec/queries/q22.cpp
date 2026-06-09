#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q22(const std::string& file) {
    auto reader =
        std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"Title", "URL", "SearchPhrase", "UserID"}));

    auto trs = TransformOperator({ColumnOperation(Transform::RegexpSearch("Google"), "Title", "title_match"),
                                  ColumnOperation(Transform::RegexpSearch("\\.google\\."), "URL", "url_google"),
                                  ColumnOperation(Transform::LogicalNot(), "url_google", "url_ok"),
                                  ColumnOperation::LogicalAnd("title_match", "url_ok", "keep")});

    FilterOperator keep("keep");
    FilterOperator phrase("SearchPhrase");

    GroupByOperator group_by({"SearchPhrase"}, {
                                                   {.tp = AggType::Min, .inp_col = "URL", .out_col = "url"},
                                                   {.tp = AggType::Min, .inp_col = "Title", .out_col = "title"},
                                                   {.tp = AggType::Count, .inp_col = "UserID", .out_col = "c"},
                                                   {.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"},
                                               });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= trs >= keep >= phrase >= group_by >= sort;
}

}  // namespace Q
