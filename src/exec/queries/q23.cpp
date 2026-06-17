#include <memory>

#include "../queries.hpp"
#include "src/core/operators.hpp"
#include "src/core/operators/chain.hpp"
#include "src/core/operators/filter.hpp"
#include "src/core/operators/select.hpp"
#include "src/core/operators/sort.hpp"
#include "src/core/skippers.hpp"
#include "src/core/types.hpp"
#include "src/exec/hits_schema.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// // SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
// std::unique_ptr<BatchStream> Q23(const std::string& file) {
//     auto reader = std::make_unique<CurseReader>(file, kHitsSchema);
//     auto sel1 = std::make_shared<TransformSelector>("URL", Transform::RegexpSearch("google"));
//     auto skip = BasicSkipper({sel1});
//     SortOperator sort({{.inp_col = "EventTime"}}, 10);
//     return std::move(reader) >= skip >= sort;
// }

// SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
std::unique_ptr<BatchStream> Q23(const std::string& file) {
    // PRIMARY KEY (CounterID, EventDate, UserID, EventTime, WatchID)
    const std::vector<std::string> pk = {"CounterID", "EventDate", "UserID", "EventTime", "WatchID"};

    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"URL", "CounterID", "EventDate", "UserID", "EventTime", "WatchID"}));

    auto sel1 = std::make_shared<TransformSelector>("URL", Transform::RegexpSearch("google"));
    auto skip = BasicSkipper({sel1});
    auto sort = SortOperator({{.inp_col = "EventTime"}}, 10);
    auto stream = std::move(reader) >= skip >= sort >= SelectOperator(pk);

    auto sub_sch = std::make_shared<Schema>(SubSchema(kHitsSchema, pk));
    TransformOperator trs({ColumnOperation::SetContains(pk, "keep", std::move(stream))});
    auto op = std::make_shared<ChainOperator>(ChainOperator::From(trs, FilterOperator("keep")));

    auto sel2 = std::make_shared<OperatorSelector>(sub_sch, op);
    auto skip2 = BasicSkipper({sel2, sel1});

    auto reader2 = std::make_unique<CurseReader>(file, kHitsSchema);
    return std::move(reader2) >= skip2 >= sort;
}

}  // namespace Q
