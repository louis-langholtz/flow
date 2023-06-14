#include <gtest/gtest.h>

#include "flow/system.hpp"

TEST(system_custom, default_construction)
{
    flow::system::custom obj;
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(empty(obj.subsystems));
    EXPECT_TRUE(empty(obj.links));
}

TEST(system_custom, equality)
{
    flow::system::custom obj_a, obj_b;
    obj_b.links = {flow::connection{}};
    EXPECT_TRUE(flow::system::custom() == flow::system::custom());
    EXPECT_TRUE(obj_a == flow::system::custom());
    EXPECT_TRUE(flow::system::custom() == obj_a);
    EXPECT_FALSE(obj_a == obj_b);
    obj_b = obj_a;
    ASSERT_TRUE(obj_a == obj_b);
    obj_b.environment = {{"twu", "one"}};
    EXPECT_FALSE(obj_a == obj_b);
}

TEST(system_executable, default_construction)
{
    flow::system::executable obj;
    EXPECT_TRUE(obj.file.empty());
    EXPECT_TRUE(obj.arguments.empty());
    EXPECT_TRUE(obj.working_directory.empty());
}

TEST(system_executable, equality)
{
    flow::system::executable obj_a, obj_b;
    obj_b.file = "/";
    EXPECT_TRUE(flow::system::executable() == flow::system::executable());
    EXPECT_TRUE(obj_a == flow::system::executable());
    EXPECT_TRUE(flow::system::executable() == obj_a);
    EXPECT_FALSE(obj_a == obj_b);
}

TEST(system, default_construction)
{
    flow::system obj;
    EXPECT_TRUE(empty(obj.interface));
    EXPECT_TRUE(std::holds_alternative<flow::system::custom>(obj.implementation));
    if (std::holds_alternative<flow::system::custom>(obj.implementation)) {
        const auto& info = std::get<flow::system::custom>(obj.implementation);
        EXPECT_TRUE(empty(info.subsystems));
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
    obj.implementation = flow::system::executable{};
    EXPECT_FALSE(obj == flow::system());
    obj.implementation = flow::system::custom{};
    ASSERT_TRUE(obj == flow::system());
}

TEST(system, executable_construction)
{
    flow::system obj{flow::system::executable{}};
    EXPECT_FALSE(empty(obj.interface));
    EXPECT_TRUE(std::holds_alternative<flow::system::executable>(obj.implementation));
    if (std::holds_alternative<flow::system::executable>(obj.implementation)) {
        const auto& info = std::get<flow::system::executable>(obj.implementation);
        EXPECT_TRUE(info.file.empty());
        EXPECT_TRUE(empty(info.arguments));
        EXPECT_TRUE(info.working_directory.empty());
    }
}
