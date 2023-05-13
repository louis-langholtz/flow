#include <gtest/gtest.h>

#include "flow/system.hpp"

using namespace flow;

TEST(system, default_construction)
{
    flow::system sys;
    EXPECT_TRUE(empty(sys.descriptors));
    EXPECT_TRUE(empty(sys.environment));
    EXPECT_TRUE(std::holds_alternative<flow::system::custom>(sys.info));
}

TEST(system, executable_construction)
{
    flow::system sys{flow::system::executable{}};
    EXPECT_FALSE(empty(sys.descriptors));
    EXPECT_TRUE(empty(sys.environment));
    EXPECT_TRUE(std::holds_alternative<flow::system::executable>(sys.info));
}
