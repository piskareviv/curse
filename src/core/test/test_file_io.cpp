// нейрослоп

#include <filesystem>
#include <memory>
#include <random>
#include <span>
#include <thread>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "src/core/file_io.hpp"

namespace fs = std::filesystem;

class FileIOTest : public ::testing::Test {
protected:
    const std::string kTestFile = "test_io_data.bin";

    // Ensure the test file is removed before and after each test
    void SetUp() override {
        if (fs::exists(kTestFile)) {
            fs::remove(kTestFile);
        }
    }
    void TearDown() override {
        if (fs::exists(kTestFile)) {
            fs::remove(kTestFile);
        }
    }
};

// Tests basic Write and Read functionality
TEST_F(FileIOTest, WriteAndReadBack) {
    const std::string expected_content = "Clang-Format is cool!";

    // Write block
    {
        curse::OfstreamWriter writer(kTestFile);
        writer.Write({expected_content.data(), expected_content.size()});
    }

    // Read block
    {
        curse::IfstreamReader reader(kTestFile);
        EXPECT_EQ(reader.GetSize(), expected_content.size());

        std::vector<char> buffer(expected_content.size());
        reader.ReadBytes(0, {buffer.data(), buffer.size()});

        std::string actual_content(buffer.begin(), buffer.end());
        EXPECT_EQ(actual_content, expected_content);
    }
}

// Tests WriteAt to ensure seeking and overwriting works
TEST_F(FileIOTest, PositionalWrite) {
    // Initial write: "01234"
    {
        curse::OfstreamWriter writer(kTestFile);
        std::string base = "01234";
        writer.Write({base.data(), base.size()});

        // Overwrite "123" with "AAA" starting at index 1
        std::string patch = "AAA";

        // writer.WriteAt(1, {patch.data(), patch.size()});
        writer.Seek(1);
        writer.Write({patch.data(), patch.size()});
    }

    // Verify: Should be "0AAA4"
    {
        curse::IfstreamReader reader(kTestFile);
        std::vector<char> buffer(5);
        reader.ReadBytes(0, buffer);
        EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "0AAA4");
    }
}

// Tests that the reader throws when the file is missing or invalid
TEST_F(FileIOTest, ThrowsOnInvalidFile) {
    // Accessing a file that hasn't been created yet
    curse::IfstreamReader reader("non_existent_file.bin");
    EXPECT_THROW(reader.GetSize(), std::runtime_error);
}

// Tests partial reads from a specific offset
TEST_F(FileIOTest, PartialReadFromOffset) {
    {
        curse::OfstreamWriter writer(kTestFile);
        writer.Write({"abcdef", 6});
    }

    curse::IfstreamReader reader(kTestFile);
    std::vector<char> buffer(2);
    // Read 2 bytes starting at offset 2 ('cd')
    reader.ReadBytes(2, buffer);

    EXPECT_EQ(buffer[0], 'c');
    EXPECT_EQ(buffer[1], 'd');
}

TEST_F(FileIOTest, ConcurrentRead) {
    const size_t file_size = 1'000'000;
    const size_t reads_per_thread = 10000;

    std::vector<char> vec(file_size);
    {
        std::mt19937 rnd(2296);
        for (char& ch : vec) {
            ch = static_cast<char>(rnd());
        }
        ASSERT_EQ(std::unordered_set<char>(vec.begin(), vec.end()).size(), 256);

        curse::OfstreamWriter writer(kTestFile);
        writer.Write(vec);
    }

    for (size_t num_threads : {1, 2, 3, 4, 10, 24, 50}) {
        // curse::IfstreamReader reader(kTestFile); // this fails (it is supposed to)
        curse::ThreadSafeFileReader reader(std::make_unique<curse::IfstreamReader>(kTestFile));

        std::vector<std::jthread> threads(num_threads);
        for (size_t i = 0; i < num_threads; i++) {
            threads[i] = std::jthread([&, i] {
                std::mt19937 rnd(i);
                for (size_t j = 0; j < reads_per_thread; j++) {
                    size_t ind = (rnd() % 100u) * (file_size / 100);
                    char ch = vec[ind];

                    std::array<char, 1> buf;
                    reader.ReadBytes(ind, buf);
                    ASSERT_EQ(buf[0], ch);
                }
            });
        }
    }
}
