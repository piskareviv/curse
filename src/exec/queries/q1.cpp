#include <cstdlib>
#include <memory>
#include <string>

#include "../queries.hpp"
#include "src/core/operators.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"
#include "src/exec/hits_schema.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q1(const std::string& file) {
    std::unique_ptr<BatchStream> reader =
        std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID"}));

    FilterOperator filt("AdvEngineID");

    AggregationOperator count(
        {AggregationOperator::Params{.tp = AggType::Count, .inp_col = "AdvEngineID", .out_col = "1"}});

    return std::move(reader) >= filt >= count;
}

}  // namespace Q
