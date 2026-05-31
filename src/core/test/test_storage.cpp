
#include <filesystem>
#include <fstream>
#include <memory>

#include "gtest/gtest.h"
#include "src/core/csv.hpp"
#include "src/core/storage.hpp"

using namespace curse;        // NOLINT
using namespace std::chrono;  // NOLINT

namespace fs = std::filesystem;

class StorageTest : public testing::Test {
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
            prf + "_3.csv",
            prf + "_4.csv",
        };

        ClearFiles();
    }

    void TearDown() override {
        ClearFiles();
    }
};

TEST_F(StorageTest, ItWorks) {
    const std::string data = "meow,123,2020-12-20\nwoof,456,2025-01-25\nwoof,456,2025-01-25\n";
    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        curse::Schema::ColumnInfo{.name = "1", .type = curse::TypeId::String},
        curse::Schema::ColumnInfo{.name = "2", .type = curse::TypeId::Int64},
        curse::Schema::ColumnInfo{.name = "3", .type = curse::TypeId::Date},
    });

    WriteAsCurse(test_files[1], std::make_unique<CsvReader>(test_files[0], schema));
    WriteAsCsv(test_files[2], std::make_unique<CurseReader>(test_files[1]));

    ASSERT_EQ(Read(test_files[2]), data);
}

TEST_F(StorageTest, CheckAllTypes) {
    const std::string data =
        "1,10,100,1000,1000000,1.234,X,hello,2020-12-20,2020-12-20 10:00:00\n"
        "0,0,0,0,0,0.1, ,world,1970-01-01,1970-01-01 00:00:00\n";

    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        {.name = "1", .type = curse::TypeId::Int8},
        {.name = "2", .type = curse::TypeId::Int16},
        {.name = "3", .type = curse::TypeId::Int32},
        {.name = "4", .type = curse::TypeId::Int64},
        {.name = "5", .type = curse::TypeId::Int128},
        {.name = "6", .type = curse::TypeId::Float64},
        {.name = "7", .type = curse::TypeId::Char},
        {.name = "8", .type = curse::TypeId::String},
        {.name = "9", .type = curse::TypeId::Date},
        {.name = "10", .type = curse::TypeId::Timestamp},
    });

    WriteAsCurse(test_files[1], std::make_unique<CsvReader>(test_files[0], schema));
    WriteAsCsv(test_files[2], std::make_unique<CsvReader>(test_files[0], schema));

    ASSERT_EQ(Read(test_files[2]), data);

    WriteAsCsv(test_files[3], std::make_unique<CurseReader>(test_files[1]));
    ASSERT_EQ(Read(test_files[3]), data);
}

TEST_F(StorageTest, MoreThanOneBatch) {
    const std::string data = "meow,123,2020-12-20\nwoof,456,2025-01-25\nwoof,456,2025-01-25\n";
    Write(test_files[0], data);

    const curse::Schema schema = curse::Schema({
        curse::Schema::ColumnInfo{.name = "1", .type = curse::TypeId::String},
        curse::Schema::ColumnInfo{.name = "2", .type = curse::TypeId::Int64},
        curse::Schema::ColumnInfo{.name = "3", .type = curse::TypeId::Date},
    });

    WriteAsCurse(test_files[1], std::make_unique<CsvReader>(test_files[0], schema, 1));
    WriteAsCsv(test_files[2], std::make_unique<CurseReader>(test_files[1]));

    ASSERT_EQ(Read(test_files[2]), data);
    ASSERT_EQ(std::make_unique<CurseReader>(test_files[1])->Next()->NRows(), 1);
}
