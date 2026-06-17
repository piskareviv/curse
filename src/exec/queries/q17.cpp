#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase LIMIT 10;
std::unique_ptr<BatchStream> Q17(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"UserID", "SearchPhrase"}));
    auto group_by =
        GroupByOperator({"UserID", "SearchPhrase"},
                        {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "cnt"}}, 10);
    return std::move(reader) >= group_by;
}

}  // namespace Q
