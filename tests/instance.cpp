#include <gtest/gtest.h>

#include "flow/instance.hpp"

TEST(instance, default_construction)
{
    flow::instance obj;
    EXPECT_TRUE(std::holds_alternative<flow::instance::custom>(obj.info));
}

TEST(instance, default_custom_construction)
{
    flow::instance::custom obj;
    EXPECT_TRUE(empty(obj.children));
    EXPECT_TRUE(empty(obj.channels));
    EXPECT_EQ(obj.pgrp, flow::instance::custom::default_pgrp);
}

TEST(instance, default_forked_construction)
{
    flow::instance::forked obj;
    EXPECT_FALSE(obj.diags.is_open());
    EXPECT_TRUE(std::holds_alternative<flow::owning_process_id>(obj.state));
    if (std::holds_alternative<flow::owning_process_id>(obj.state)) {
        const auto& pid = std::get<flow::owning_process_id>(obj.state);
        EXPECT_EQ(flow::reference_process_id(pid),
                  flow::owning_process_id::default_process_id);
    }
}
