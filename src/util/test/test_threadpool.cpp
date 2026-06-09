#include <future>

#include "gtest/gtest.h"
#include "src/util/thread_pool.hpp"

using namespace curse;  // NOLINT

TEST(ThreadPool, ItWorks) {
    const size_t n = 1e4;

    std::vector<int> vec(n);
    std::vector<std::future<void>> futures(n);

    for (size_t i = 0; i < n; i++) {
        futures[i] = thread_pool.Push([&vec, i] { vec[i] = i; });
    }
    for (size_t i = 0; i < n; i++) {
        futures[i].wait();
    }
    for (size_t i = 0; i < n; i++) {
        ASSERT_EQ(vec[i], i);
    }
}
