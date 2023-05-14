#include <gtest/gtest.h>

#include "flow/system.hpp"

TEST(system_custom, default_construction)
{
    flow::system::custom obj;
    EXPECT_TRUE(empty(obj.subsystems));
    EXPECT_TRUE(empty(obj.connections));
}

TEST(system_custom, equality)
{
    flow::system::custom obj_a, obj_b;
    obj_b.connections = {flow::connection{}};
    EXPECT_TRUE(flow::system::custom() == flow::system::custom());
    EXPECT_TRUE(obj_a == flow::system::custom());
    EXPECT_TRUE(flow::system::custom() == obj_a);
    EXPECT_FALSE(obj_a == obj_b);
}

TEST(system_executable, default_construction)
{
    flow::system::executable obj;
    EXPECT_TRUE(obj.executable_file.empty());
    EXPECT_TRUE(obj.arguments.empty());
    EXPECT_TRUE(obj.working_directory.empty());
}

TEST(system_executable, equality)
{
    flow::system::executable obj_a, obj_b;
    obj_b.executable_file = "/";
    EXPECT_TRUE(flow::system::executable() == flow::system::executable());
    EXPECT_TRUE(obj_a == flow::system::executable());
    EXPECT_TRUE(flow::system::executable() == obj_a);
    EXPECT_FALSE(obj_a == obj_b);
}

TEST(system, default_construction)
{
    flow::system obj;
    EXPECT_TRUE(empty(obj.descriptors));
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::system::custom>(obj.info));
    if (std::holds_alternative<flow::system::custom>(obj.info)) {
        const auto& info = std::get<flow::system::custom>(obj.info);
        EXPECT_TRUE(empty(info.subsystems));
        EXPECT_TRUE(empty(info.connections));
    }
}

TEST(system, equality)
{
    EXPECT_TRUE(flow::system() == flow::system());
    flow::system obj;
    EXPECT_TRUE(obj == flow::system());
    obj.environment = {{"twu", "one"}};
    EXPECT_FALSE(obj == flow::system());
    obj.environment = {};
    ASSERT_TRUE(obj == flow::system());
    obj.descriptors = flow::std_descriptors;
    EXPECT_FALSE(obj == flow::system());
    obj.descriptors = {};
    ASSERT_TRUE(obj == flow::system());
    obj.info = flow::system::executable{};
    EXPECT_FALSE(obj == flow::system());
    obj.info = flow::system::custom{};
    ASSERT_TRUE(obj == flow::system());
}

TEST(system, executable_construction)
{
    flow::system obj{flow::system::executable{}};
    EXPECT_FALSE(empty(obj.descriptors));
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::system::executable>(obj.info));
    if (std::holds_alternative<flow::system::executable>(obj.info)) {
        const auto& info = std::get<flow::system::executable>(obj.info);
        EXPECT_TRUE(info.executable_file.empty());
        EXPECT_TRUE(empty(info.arguments));
        EXPECT_TRUE(info.working_directory.empty());
    }
}
