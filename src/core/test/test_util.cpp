#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

#include "gtest/gtest.h"
#include "src/core/util.hpp"

TEST(Concat, ItWorks) {
    std::vector<int> a = curse::Concat(std::vector<int>{1, 2}, std::vector<int>{3, 4, 5, 6});
    std::vector<int> b = std::vector<int>{1, 2, 3, 4, 5, 6};
    ASSERT_EQ(a, b);
}

TEST(Concat, ManyArgs) {
    std::vector<int> a1 = std::vector<int>{1, 2};
    std::vector<int> a2 = std::vector<int>{3, 4};
    std::vector<int> a3 = std::vector<int>{5, 6};
    std::vector<int> a4 = std::vector<int>{7, 8};
    std::vector<int> b = std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_EQ(curse::Concat(a1, a2, a3, a4), b);
}

TEST(ToFromBytes, ItWorks) {
    ASSERT_EQ(1, curse::ValueFromBytes<int>(curse::ValueToBytes(1)));
    ASSERT_EQ(-1, curse::ValueFromBytes<int64_t>(curse::ValueToBytes<int64_t>(-1)));

    std::vector<int> vec1 = {1, 2, 3, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    ASSERT_EQ(vec1, curse::VecFromBytes<int>(curse::VecToBytes(vec1)));

    std::vector<char> vec2 = {'a', 'b', 'a', 'c', 'a', 'b', 'a'};
    ASSERT_EQ(vec2, curse::VecFromBytes<char>(curse::VecToBytes(vec2)));

    std::vector<int64_t> vec3 = {1, 2, 3, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    ASSERT_EQ(vec3, curse::VecFromBytes<int64_t>(curse::VecToBytes(vec3)));
}

TEST(SplitSpan, ItWorks) {
    std::vector<int> vec = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> x = {1, 2, 3};
    std::vector<int> y = {4, 5, 6, 7, 8, 9, 10};

    auto [a, b] = curse::SplitSpan(std::span<const int>(vec), 3);

    ASSERT_EQ(std::vector(a.begin(), a.end()), x);
    ASSERT_EQ(std::vector(b.begin(), b.end()), y);
}
