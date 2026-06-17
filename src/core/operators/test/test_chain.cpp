#include <memory>

#include "gtest/gtest.h"
#include "src/core/csv.hpp"
#include "src/core/operators/chain.hpp"
#include "src/core/operators/count.hpp"
#include "src/core/operators/groupby.hpp"
#include "src/core/types.hpp"

using namespace curse;  // NOLINT

TEST(Operators_Chain, ItWorks) {
    const std::string data =
        "meow,2296\n"
        "meow,2296\n"
        "oh no,2296\n"
        "oh no,2296\n"
        "oh no,1234\n"
        "oh no,2296\n"
        "oh no,2296\n"
        "oh no,2296\n"
        "oh no,2296\n"
        "woof,2296\n";

    const Schema schema = Schema({
        Schema::ColumnInfo{.name = "1", .type = TypeId::String},
        Schema::ColumnInfo{.name = "2", .type = TypeId::Int64},
    });

    for (size_t batch_sz = 1; batch_sz <= 1000; batch_sz *= 2) {
        std::unique_ptr<std::istringstream> sin = std::make_unique<std::istringstream>(data);
        std::unique_ptr<BatchStream> reader = std::make_unique<CsvReader>(std::move(sin), schema, batch_sz);

        GroupByOperator groupby({"1", "2"}, {});
        CountOperator count("ljsdflasjdf");

        ChainOperator chain = ChainOperator::From(groupby, count);

        std::unique_ptr<BatchStream> stream = std::move(reader) >= chain;

        std::ostringstream sout;
        WriteAsCsv(sout, std::move(stream));

        std::string result = sout.str();

        const std::string expected_result = "4\n";

        ASSERT_EQ(result, expected_result);
    }
}
