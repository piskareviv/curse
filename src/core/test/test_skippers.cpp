#include <filesystem>
#include <memory>
#include <random>
#include <span>
#include <thread>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "src/core/csv.hpp"
#include "src/core/operators/transform.hpp"
#include "src/core/skippers.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"

namespace fs = std::filesystem;

class SkippersTest : public ::testing::Test {
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
            prf + "_1.bin",
            prf + "_2.bin",
        };

        ClearFiles();
    }

    void TearDown() override {
        ClearFiles();
    }
};
using namespace curse;  // NOLINT

TEST_F(SkippersTest, ItWorks) {
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

    const std::string test_file = test_files[0];

    const Schema schema = Schema({
        Schema::ColumnInfo{.name = "1", .type = TypeId::String},
        Schema::ColumnInfo{.name = "2", .type = TypeId::Int64},
    });

    for (size_t batch_sz = 1; batch_sz <= 100; batch_sz += 5) {
        WriteAsCurse(test_file,
                     std::make_unique<CsvReader>(std::make_unique<std::istringstream>(data), schema, batch_sz));

        std::unique_ptr<BatchViewStream> reader = std::make_unique<CurseReader>(test_file);

        std::shared_ptr<TransformSelector> sel1 = std::make_shared<TransformSelector>(
            "2", Transform::Compare(curse::Transform::ComparisonType::Equal, Value::From<curse::TypeId::Int64>(2296)));
        std::shared_ptr<TransformSelector> sel2 = std::make_shared<TransformSelector>(
            "1",
            Transform::Compare(curse::Transform::ComparisonType::NotEqual, Value::From<curse::TypeId::String>("woof")));

        BasicSkipper skip({sel1, sel2});

        std::unique_ptr<BatchStream> stream = std::move(reader) >= skip;

        std::ostringstream sout;
        WriteAsCsv(sout, std::move(stream));

        std::string result = sout.str();

        const std::string expected_result =
            "meow,2296\n"
            "meow,2296\n"
            "oh no,2296\n"
            "oh no,2296\n"
            "oh no,2296\n"
            "oh no,2296\n"
            "oh no,2296\n"
            "oh no,2296\n";
        ASSERT_EQ(result, expected_result);
    }
}
