#include <gtest/gtest.h>

#include "gtest/gtest.h"
#include "src/tmp/tmp_lib.hpp"

TEST(ItWorks, ItWorks) {
    testing::internal::CaptureStdout();

    it_works();

    std::string output = testing::internal::GetCapturedStdout();

    // ASSERT_EQ(output, "it works");  // wrong
    ASSERT_EQ(output, "it works\n");
}

// TEST(UB, UB) {
//     volatile int a = 100000;
//     volatile int b = a * a;
//     (void)b;
// }
