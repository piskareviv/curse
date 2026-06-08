#include "gtest/gtest.h"
#include "src/core/assert.hpp"

TEST(MyAssert, ItWorks) {
    ENSURE(true);
    ENSURE(1 || 0);  // parenthesis
    EXPECT_ANY_THROW(ENSURE(false));
}
