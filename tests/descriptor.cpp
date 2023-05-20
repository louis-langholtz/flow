#include <gtest/gtest.h>

#include "flow/descriptor.hpp"

using namespace flow;

TEST(descriptor, default_construction)
{
    ASSERT_NO_THROW(descriptor());
    EXPECT_TRUE(std::holds_alternative<owning_descriptor>(descriptor()));
}
