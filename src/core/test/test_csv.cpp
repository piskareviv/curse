#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>

#include "gtest/gtest.h"
#include "src/core/csv.hpp"
#include "src/core/types.hpp"

using namespace curse;        // NOLINT
using namespace std::chrono;  // NOLINT

namespace fs = std::filesystem;

class CSV_IO_Test : public testing::Test {  // NOLINT
protected:
    std::vector<std::string> test_files;

    void ClearFiles() {
        for (auto file : test_files) {
            if (fs::exists(file)) {
                fs::remove(file);
            }
        }
    }

    bool CheckFile(const std::string& file) {
        return std::count(test_files.begin(), test_files.end(), file) > 0;
    }

    void Write(const std::string& file, const std::string& s) {
        if (!CheckFile(file)) {
            return;
        }

        std::ofstream fout(file, std::ios::binary);
        fout.write(s.data(), s.size());
    }

    std::string Read(const std::string& file) {
        if (!CheckFile(file)) {
            return "";
        }

        size_t size = fs::file_size(file);
        std::string s(size, '-');
        std::ifstream fin(file, std::ios::binary);
        fin.read(s.data(), size);
        return s;
    }

    void SetUp() override {
        auto info = testing::UnitTest::GetInstance()->current_test_info();
        std::string prf = std::string(info->test_suite_name()) + "_" + info->name();

        test_files = {
            prf + "_1.csv",
            prf + "_2.csv",
        };

        ClearFiles();
    }

    void TearDown() override {
        ClearFiles();
    }
};

TEST_F(CSV_IO_Test, InputWorks) {
    const std::string data = "meow,123,2020-12-20\nwoof,456,2025-01-25\nwoof,456,2025-01-25\n";
    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        curse::Schema::ColumnInfo{.name = "1", .type = curse::TypeId::String},
        curse::Schema::ColumnInfo{.name = "2", .type = curse::TypeId::Int64},
        curse::Schema::ColumnInfo{.name = "3", .type = curse::TypeId::Date},
    });

    curse::CsvReader reader(test_files[0], schema);

    ASSERT_EQ(schema.Columns()[0].name, reader.GetSchema()->Columns()[0].name);

    curse::Batch batch = *reader.Next();

    ASSERT_EQ(batch.NRows(), 3);
    ASSERT_EQ(batch.Columns()[0].Values().index(), size_t(curse::TypeId::String));

    ASSERT_EQ(std::get<size_t(curse::TypeId::String)>(batch.Columns()[0].Values())[0], std::string("meow"));
    ASSERT_EQ(std::get<curse::ColumnT<curse::TypeId::String>>(batch.Columns()[0].Values())[0], std::string("meow"));

    std::vector<std::string> col1 = {"meow", "woof", "woof"};

    std::vector<int64_t> col2 = {123, 456, 456};

    std::vector<std::chrono::year_month_day> col3 = {
        std::chrono::year_month_day(2020y, December, 20d),
        std::chrono::year_month_day(2025y, January, 25d),
        std::chrono::year_month_day(2025y, January, 25d),
    };

    ASSERT_EQ(std::get<curse::ColumnT<curse::TypeId::String>>(batch.Columns()[0].Values()).values, col1);
    ASSERT_EQ(std::get<curse::ColumnT<curse::TypeId::Int64>>(batch.Columns()[1].Values()).values, col2);
    ASSERT_EQ(std::get<curse::ColumnT<curse::TypeId::Date>>(batch.Columns()[2].Values()).values, col3);

    ASSERT_ANY_THROW(std::get<curse::ColumnT<curse::TypeId::Timestamp>>(batch.Columns()[2].Values()));
}

TEST_F(CSV_IO_Test, OutputWorks) {
    const std::string data = "meow,123,2020-12-20\nwoof,456,2025-01-25\nwoof,456,2025-01-25\n";
    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        curse::Schema::ColumnInfo{.name = "1", .type = curse::TypeId::String},
        curse::Schema::ColumnInfo{.name = "2", .type = curse::TypeId::Int64},
        curse::Schema::ColumnInfo{.name = "3", .type = curse::TypeId::Date},
    });

    curse::WriteAsCsv(test_files[1], std::make_unique<CsvReader>(test_files[0], schema));

    ASSERT_EQ(Read(test_files[1]), data);
}

TEST_F(CSV_IO_Test, CheckAllTypes) {
    const std::string data =
        "1,200,30000,4000000000,5000000000000000000,3.14,A,hello,2023-10-05,2023-10-05 12:00:00\n"
        "-1,-200,-30000,-4000000000,-5000000000000000000,2.718,B,world,2024-01-01,2024-01-01 00:00:00.123456789\n";

    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        curse::Schema::ColumnInfo{.name = "1", .type = curse::TypeId::Int8},
        curse::Schema::ColumnInfo{.name = "2", .type = curse::TypeId::Int16},
        curse::Schema::ColumnInfo{.name = "3", .type = curse::TypeId::Int32},
        curse::Schema::ColumnInfo{.name = "4", .type = curse::TypeId::Int64},
        curse::Schema::ColumnInfo{.name = "5", .type = curse::TypeId::Int128},
        curse::Schema::ColumnInfo{.name = "6", .type = curse::TypeId::Float64},
        curse::Schema::ColumnInfo{.name = "7", .type = curse::TypeId::Char},
        curse::Schema::ColumnInfo{.name = "8", .type = curse::TypeId::String},
        curse::Schema::ColumnInfo{.name = "9", .type = curse::TypeId::Date},
        curse::Schema::ColumnInfo{.name = "10", .type = curse::TypeId::Timestamp},
    });

    curse::WriteAsCsv(test_files[1], std::make_unique<curse::CsvReader>(test_files[0], schema));

    ASSERT_EQ(Read(test_files[0]), data);
}

TEST_F(CSV_IO_Test, EscapedNewline) {
    const std::string data = "123,\"must\nkys\"\n";

    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        curse::Schema::ColumnInfo{.name = "1", .type = curse::TypeId::Int8},
        curse::Schema::ColumnInfo{.name = "2", .type = curse::TypeId::String},
    });

    curse::WriteAsCsv(test_files[1], std::make_unique<curse::CsvReader>(test_files[0], schema));

    ASSERT_EQ(Read(test_files[1]), data);

    curse::CsvReader reader(test_files[1], schema);
    curse::Batch batch = *reader.Next();

    ASSERT_EQ(std::get<curse::ColumnT<curse::TypeId::String>>(batch.Columns()[1].Values())[0],
              std::string("must\nkys"));
}

TEST_F(CSV_IO_Test, IntegerMinMaxValues) {
    const std::string data =
        "127,32767,2147483647,9223372036854775807,170141183460469231731687303715884105727\n"
        "-128,-32768,-2147483648,-9223372036854775808,-170141183460469231731687303715884105728\n";

    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        curse::Schema::ColumnInfo{.name = "1", .type = curse::TypeId::Int8},
        curse::Schema::ColumnInfo{.name = "2", .type = curse::TypeId::Int16},
        curse::Schema::ColumnInfo{.name = "3", .type = curse::TypeId::Int32},
        curse::Schema::ColumnInfo{.name = "4", .type = curse::TypeId::Int64},
        curse::Schema::ColumnInfo{.name = "5", .type = curse::TypeId::Int128},
    });

    curse::WriteAsCsv(test_files[1], std::make_unique<curse::CsvReader>(test_files[0], schema));

    ASSERT_EQ(Read(test_files[1]), data);
}

TEST_F(CSV_IO_Test, ZeroValueBytes) {
    std::string data;
    data += '\0';
    data += ",";
    data += ",";
    data += "a";
    data += '\0';
    data += "b";
    data += "\n";

    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        curse::Schema::ColumnInfo{.name = "1", .type = curse::TypeId::Char},
        curse::Schema::ColumnInfo{.name = "2", .type = curse::TypeId::String},
        curse::Schema::ColumnInfo{.name = "3", .type = curse::TypeId::String},
    });

    curse::WriteAsCsv(test_files[1], std::make_unique<curse::CsvReader>(test_files[0], schema));

    std::string result = Read(test_files[1]);
    ASSERT_EQ(result, data);
}
