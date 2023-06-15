#include <gtest/gtest.h>

#include "flow/node.hpp"

TEST(node_system, default_construction)
{
    flow::system obj;
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(empty(obj.nodes));
    EXPECT_TRUE(empty(obj.links));
}

TEST(node_system, equality)
{
    flow::system obj_a, obj_b;
    obj_b.links = {flow::link{}};
    EXPECT_TRUE(flow::system() == flow::system());
    EXPECT_TRUE(obj_a == flow::system());
    EXPECT_TRUE(flow::system() == obj_a);
    EXPECT_FALSE(obj_a == obj_b);
    obj_b = obj_a;
    ASSERT_TRUE(obj_a == obj_b);
    obj_b.environment = {{"twu", "one"}};
    EXPECT_FALSE(obj_a == obj_b);
}

TEST(node_executable, default_construction)
{
    flow::executable obj;
    EXPECT_TRUE(obj.file.empty());
    EXPECT_TRUE(obj.arguments.empty());
    EXPECT_TRUE(obj.working_directory.empty());
}

TEST(node_executable, equality)
{
    flow::executable obj_a, obj_b;
    obj_b.file = "/";
    EXPECT_TRUE(flow::executable() == flow::executable());
    EXPECT_TRUE(obj_a == flow::executable());
    EXPECT_TRUE(flow::executable() == obj_a);
    EXPECT_FALSE(obj_a == obj_b);
}

TEST(node, default_construction)
{
    flow::node obj;
    EXPECT_TRUE(empty(obj.interface));
    EXPECT_TRUE(std::holds_alternative<flow::system>(obj.implementation));
    if (std::holds_alternative<flow::system>(obj.implementation)) {
        const auto& info = std::get<flow::system>(obj.implementation);
        EXPECT_TRUE(empty(info.nodes));
        EXPECT_TRUE(empty(info.links));
    }
}

TEST(node, equality)
{
    EXPECT_TRUE(flow::node() == flow::node());
    flow::node obj;
    EXPECT_TRUE(obj == flow::node());
    obj.interface = flow::std_ports;
    EXPECT_FALSE(obj == flow::node());
    obj.interface = {};
    ASSERT_TRUE(obj == flow::node());
    obj.implementation = flow::executable{};
    EXPECT_FALSE(obj == flow::node());
    obj.implementation = flow::system{};
    ASSERT_TRUE(obj == flow::node());
}

TEST(node, executable_construction)
{
    flow::node obj{flow::executable{}};
    EXPECT_FALSE(empty(obj.interface));
    EXPECT_TRUE(std::holds_alternative<flow::executable>(obj.implementation));
    if (std::holds_alternative<flow::executable>(obj.implementation)) {
        const auto& info = std::get<flow::executable>(obj.implementation);
        EXPECT_TRUE(info.file.empty());
        EXPECT_TRUE(empty(info.arguments));
        EXPECT_TRUE(info.working_directory.empty());
    }
}
