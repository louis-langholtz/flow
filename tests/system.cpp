#include <gtest/gtest.h>

#include "flow/system.hpp"

TEST(system, default_construction)
{
    flow::system sys;
    EXPECT_TRUE(empty(sys.descriptors));
    EXPECT_TRUE(empty(sys.environment));
    EXPECT_TRUE(std::holds_alternative<flow::system::custom>(sys.info));
    if (std::holds_alternative<flow::system::custom>(sys.info)) {
        const auto& info = std::get<flow::system::custom>(sys.info);
        EXPECT_TRUE(empty(info.subsystems));
        EXPECT_TRUE(empty(info.connections));
    }
}

TEST(system, executable_construction)
{
    flow::system sys{flow::system::executable{}};
    EXPECT_FALSE(empty(sys.descriptors));
    EXPECT_TRUE(empty(sys.environment));
    EXPECT_TRUE(std::holds_alternative<flow::system::executable>(sys.info));
    if (std::holds_alternative<flow::system::executable>(sys.info)) {
        const auto& info = std::get<flow::system::executable>(sys.info);
        EXPECT_TRUE(info.executable_file.empty());
        EXPECT_TRUE(empty(info.arguments));
        EXPECT_TRUE(info.working_directory.empty());
    }
}
