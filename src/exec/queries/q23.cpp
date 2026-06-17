#include <memory>

#include "../queries.hpp"
#include "src/core/operators/transform.hpp"
#include "src/core/skippers.hpp"
#include "src/core/types.hpp"
#include "src/exec/hits_schema.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

// SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
std::unique_ptr<BatchStream> Q23(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, kHitsSchema);
    auto sel1 = std::make_shared<TransformSelector>("URL", Transform::RegexpSearch("google"));
    auto skip = BasicSkipper({sel1});
    SortOperator sort({{.inp_col = "EventTime"}}, 10);
    return std::move(reader) >= skip >= sort;
}

// // SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
// std::unique_ptr<BatchStream> Q23(const std::string& file) {
//     auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"URL", "EventTime", "UserID"}));
//     auto sel1 = std::make_shared<TransformSelector>("URL", Transform::RegexpSearch("google"));
//     auto skip = BasicSkipper({sel1});
//     SortOperator sort({{.inp_col = "EventTime"}}, 10);

//     auto stream = std::move(reader) >= skip >= sort;
//     // auto sel2 = std::make_shared<TransformSelector>(""ColumnOperation::SetContains({"EventTime", "UserID"},
//     "keep", std::move(stream))) auto sel2 = BasicSkipper{};

//     auto reader2 = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"URL", "EventTime", "UserID"}));
//     return std::move(reader2) >=
// }

}  // namespace Q
