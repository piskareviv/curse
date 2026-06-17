#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT MobilePhone, MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE MobilePhoneModel <> '' GROUP BY MobilePhone, MobilePhoneModel ORDER BY u DESC LIMIT 10;
std::unique_ptr<BatchStream> Q11(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(
        file, SubSchema(kHitsSchema, {"MobilePhone", "MobilePhoneModel", "UserID"}));

    auto filter = FilterOperator("MobilePhoneModel");

    auto group_by =
        GroupByOperator({"MobilePhone", "MobilePhoneModel"},
                        {GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});

    auto sort = SortOperator({{.inp_col = "u", .reversed = true}}, 10);

    return std::move(reader) >= filter >= group_by >= sort;
}

}  // namespace Q
