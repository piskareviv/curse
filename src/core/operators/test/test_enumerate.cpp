#include <memory>
#include <sstream>
#include <string>

#include "gtest/gtest.h"
#include "src/core/convert.hpp"
#include "src/core/csv.hpp"
#include "src/core/operators/enumerate.hpp"
#include "src/core/types.hpp"

using namespace curse;  // NOLINT

TEST(Operators_Enumerate, ItWorks) {
    const std::string data =
        "meow,2296\n"
        "meow,2296\n"
        "woof,2296\n";

    const Schema schema = Schema({
        Schema::ColumnInfo{.name = "1", .type = TypeId::String},
        Schema::ColumnInfo{.name = "2", .type = TypeId::Int64},
    });

    std::unique_ptr<std::istringstream> sin = std::make_unique<std::istringstream>(data);
    std::unique_ptr<BatchStream> reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    EnumerateOperator enumerate("3");

    std::unique_ptr<BatchStream> stream = std::move(reader) >= enumerate;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    std::string result = sout.str();

    const std::string expected_result =
        "meow,2296,0\n"
        "meow,2296,1\n"
        "woof,2296,2\n";

    ASSERT_EQ(result, expected_result);
}

TEST(Operators_Enumerate, Batches) {
    const std::string data = [&] {
        std::string s;
        for (size_t i = 0; i < 1000; i++) {
            s += "meow\n";
        }
        return s;
    }();

    const Schema schema = Schema({
        Schema::ColumnInfo{.name = "1", .type = TypeId::String},
    });

    std::unique_ptr<std::istringstream> sin = std::make_unique<std::istringstream>(data);
    std::unique_ptr<BatchStream> reader = std::make_unique<CsvReader>(std::move(sin), schema, 100);

    EnumerateOperator enumerate("2");

    std::unique_ptr<BatchStream> stream = std::move(reader) >= enumerate;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    std::string result = sout.str();

    const std::string expected_result = [&] {
        std::string s;
        for (size_t i = 0; i < 1000; i++) {
            s += "meow," + std::to_string(i) + "\n";
        }
        return s;
    }();

    ASSERT_EQ(result, expected_result);
}

TEST(Operators_Enumerate, Int128) {
    const std::string data =
        "meow,2296\n"
        "meow,2296\n"
        "woof,2296\n";

    const Schema schema = Schema({
        Schema::ColumnInfo{.name = "1", .type = TypeId::String},
        Schema::ColumnInfo{.name = "2", .type = TypeId::Int64},
    });

    std::unique_ptr<std::istringstream> sin = std::make_unique<std::istringstream>(data);
    std::unique_ptr<BatchStream> reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    EnumerateOperator enumerate(
        "3", Value::From<curse::TypeId::Int128>(ConvertVal<TypeId::Int128>::FromString("1000000000000000000000000")));

    std::unique_ptr<BatchStream> stream = std::move(reader) >= enumerate;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    std::string result = sout.str();

    const std::string expected_result =
        "meow,2296,1000000000000000000000000\n"
        "meow,2296,1000000000000000000000001\n"
        "woof,2296,1000000000000000000000002\n";

    ASSERT_EQ(result, expected_result);
}

TEST(Operators_Enumerate, NegativeStart) {
    const std::string data = [&] {
        std::string s;
        for (size_t i = 0; i < 1000; i++) {
            s += "meow\n";
        }
        return s;
    }();

    const Schema schema = Schema({
        Schema::ColumnInfo{.name = "1", .type = TypeId::String},
    });

    std::unique_ptr<std::istringstream> sin = std::make_unique<std::istringstream>(data);
    std::unique_ptr<BatchStream> reader = std::make_unique<CsvReader>(std::move(sin), schema, 100);

    EnumerateOperator enumerate("2", -500);

    std::unique_ptr<BatchStream> stream = std::move(reader) >= enumerate;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    std::string result = sout.str();

    const std::string expected_result = [&] {
        std::string s;
        for (size_t i = 0; i < 1000; i++) {
            s += "meow," + std::to_string(-500 + static_cast<int>(i)) + "\n";
        }
        return s;
    }();

    ASSERT_EQ(result, expected_result);
}
