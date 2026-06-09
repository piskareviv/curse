#include <span>
#include <vector>

#include "gtest/gtest.h"
#include "src/util/lz4.hpp"

std::vector<char> Compress(std::span<const char> span) {
    std::vector<char> vec;
    curse::CompressLZ4(span, vec);
    return vec;
}

std::vector<char> Decompress(std::span<const char> span) {
    std::vector<char> vec;
    curse::DecompressLZ4(span, vec);
    return vec;
}

TEST(LZ4_API, ItWorks) {
    const std::string data =
        "abacabadabacabaeabacabadabacabafabacabadabacabaeabacabadabacabagabacabadabacabaeabacabadabacabafabacabadabacab"
        "aeabacabadabacaba";
    std::vector<char> vec(data.begin(), data.end());

    ASSERT_EQ(vec, Decompress(Compress(vec)));
    ASSERT_LE(Compress(vec).size() * 2, vec.size());
}
