#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>

#include "src/core/csv.hpp"
#include "src/core/operators.hpp"
#include "src/core/operators/count.hpp"
#include "src/core/operators/filter.hpp"
#include "src/core/operators/transform.hpp"
#include "src/core/skippers.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"
#include "src/exec/queries.hpp"
#include "src/util/assert.hpp"

using namespace curse;  // NOLINT

// SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
std::unique_ptr<BatchStream> ExecuteQuery(const std::string& file) {
    auto reader = std::make_unique<SimpleCurseReader>(file, SubSchema(kHitsSchema, {"URL"}));
    auto match = TransformOperator({ColumnOperation("URL", "keep", Transform::RegexpSearch("google"))});
    auto filt = FilterOperator("keep");
    auto count = CountOperator("1");
    return std::move(reader) >= count;
    // return std::move(reader) >= match >= filt >= count;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << std::format("usage: {} <CURSED FILE> <OUTPUT> \n", argv[0]) << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    std::unique_ptr<curse::BatchStream> output_stream = ExecuteQuery(input_file);

    if (output_file != "-") {
        curse::WriteAsCsv(output_file, std::move(output_stream));
    } else {
        curse::WriteAsCsv(std::cout, std::move(output_stream));
    }

    return 0;
}
