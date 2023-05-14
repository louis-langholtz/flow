#include <gtest/gtest.h>

#include "flow/instance.hpp"

TEST(instance, default_construction)
{
    flow::instance obj;
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::instance::custom>(obj.info));
}
