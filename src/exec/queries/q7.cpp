#include "../queries.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q7(const std::string& file) {
    std::unique_ptr<BatchStream> reader =
        std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID"}));

    FilterOperator filt("AdvEngineID");

    GroupByOperator group_by(
        {"AdvEngineID"}, {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "AdvEngineID", .out_col = "cnt"}});

    SortOperator sort({SortOperator::Params{.inp_col = "cnt", .reversed = true}});

    return std::move(reader) >= filt >= group_by >= sort;
}

}  // namespace Q
