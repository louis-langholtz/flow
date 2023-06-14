#include <gtest/gtest.h>

#include "flow/system.hpp"

TEST(system_custom, default_construction)
{
    flow::custom obj;
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(empty(obj.nodes));
    EXPECT_TRUE(empty(obj.links));
}

TEST(system_custom, equality)
{
    flow::custom obj_a, obj_b;
    obj_b.links = {flow::connection{}};
    EXPECT_TRUE(flow::custom() == flow::custom());
    EXPECT_TRUE(obj_a == flow::custom());
    EXPECT_TRUE(flow::custom() == obj_a);
    EXPECT_FALSE(obj_a == obj_b);
    obj_b = obj_a;
    ASSERT_TRUE(obj_a == obj_b);
    obj_b.environment = {{"twu", "one"}};
    EXPECT_FALSE(obj_a == obj_b);
}

TEST(system_executable, default_construction)
{
    flow::executable obj;
    EXPECT_TRUE(obj.file.empty());
    EXPECT_TRUE(obj.arguments.empty());
    EXPECT_TRUE(obj.working_directory.empty());
}

TEST(system_executable, equality)
{
    flow::executable obj_a, obj_b;
    obj_b.file = "/";
    EXPECT_TRUE(flow::executable() == flow::executable());
    EXPECT_TRUE(obj_a == flow::executable());
    EXPECT_TRUE(flow::executable() == obj_a);
    EXPECT_FALSE(obj_a == obj_b);
}

TEST(system, default_construction)
{
    flow::system obj;
    EXPECT_TRUE(empty(obj.interface));
    EXPECT_TRUE(std::holds_alternative<flow::custom>(obj.implementation));
    if (std::holds_alternative<flow::custom>(obj.implementation)) {
        const auto& info = std::get<flow::custom>(obj.implementation);
        EXPECT_TRUE(empty(info.nodes));
        EXPECT_TRUE(empty(info.links));
    }
}

TEST(system, equality)
{
    EXPECT_TRUE(flow::system() == flow::system());
    flow::system obj;
    EXPECT_TRUE(obj == flow::system());
    obj.interface = flow::std_ports;
    EXPECT_FALSE(obj == flow::system());
    obj.interface = {};
    ASSERT_TRUE(obj == flow::system());
    obj.implementation = flow::executable{};
    EXPECT_FALSE(obj == flow::system());
    obj.implementation = flow::custom{};
    ASSERT_TRUE(obj == flow::system());
}

TEST(system, executable_construction)
{
    flow::system obj{flow::executable{}};
    EXPECT_FALSE(empty(obj.interface));
    EXPECT_TRUE(std::holds_alternative<flow::executable>(obj.implementation));
    if (std::holds_alternative<flow::executable>(obj.implementation)) {
        const auto& info = std::get<flow::executable>(obj.implementation);
        EXPECT_TRUE(info.file.empty());
        EXPECT_TRUE(empty(info.arguments));
        EXPECT_TRUE(info.working_directory.empty());
    }
}
