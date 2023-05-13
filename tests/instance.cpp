#include <sstream> // for std::ostringstream

#include <gtest/gtest.h>

#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

TEST(instance, default_construction)
{
    flow::instance obj;
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::instance::custom>(obj.info));
}

TEST(instance, instantiate_default_custom)
{
    std::ostringstream os;
    const auto obj = instantiate(flow::system_name{},
                                 flow::system::custom{}, os);
    EXPECT_TRUE(empty(os.str()));
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::instance::custom>(obj.info));
    if (std::holds_alternative<flow::instance::custom>(obj.info)) {
        const auto& info = std::get<flow::instance::custom>(obj.info);
        EXPECT_TRUE(empty(info.children));
        EXPECT_TRUE(empty(info.channels));
        EXPECT_EQ(info.pgrp, flow::no_process_id);
    }
}

TEST(instance, instantiate_default_executable)
{
    std::ostringstream os;
    EXPECT_THROW(instantiate(flow::system_name{}, flow::system::executable{},
                             os),
                 std::invalid_argument);
    EXPECT_TRUE(empty(os.str()));
}

TEST(instance, instantiate_empty_executable)
{
    const auto expected_msg =
        "execve of '' failed: system:2 (No such file or directory)\n";
    const auto sys = flow::system{flow::system::executable{}, {}, {}};
    std::ostringstream os;
    auto obj = instantiate(flow::system_name{}, sys, os);
    EXPECT_TRUE(empty(os.str()));
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::instance::forked>(obj.info));
    if (std::holds_alternative<flow::instance::forked>(obj.info)) {
        auto& info = std::get<flow::instance::forked>(obj.info);
        EXPECT_TRUE(info.diags.is_open());
        EXPECT_TRUE(info.diags.good());
        EXPECT_TRUE(std::holds_alternative<flow::owning_process_id>(info.state));
        if (std::holds_alternative<flow::owning_process_id>(info.state)) {
            auto& state = std::get<flow::owning_process_id>(info.state);
            const auto pid = int(state);
            EXPECT_GT(pid, 0);
            const auto result = state.wait();
            EXPECT_EQ(result.type(), flow::wait_result::has_info);
            EXPECT_EQ(flow::invalid_process_id,
                      flow::reference_process_id(state));
            if (result.holds_info()) {
                const auto info = result.info();
                EXPECT_EQ(flow::reference_process_id(pid), info.id);
                EXPECT_TRUE(std::holds_alternative<flow::wait_exit_status>(info.status));
                if (const auto p = std::get_if<flow::wait_exit_status>(&info.status)) {
                    EXPECT_EQ(p->value, 1);
                }
            }
            info.diags.seekg(0, std::ios_base::end);
            EXPECT_TRUE(info.diags.good());
            EXPECT_EQ(info.diags.tellg(), 58);
            EXPECT_TRUE(info.diags.good());
            info.diags.seekg(0, std::ios_base::beg);
            os.clear();
            std::copy(std::istreambuf_iterator<char>(info.diags),
                      std::istreambuf_iterator<char>(),
                      std::ostream_iterator<char>(os));
            EXPECT_EQ(os.str(), std::string(expected_msg));
        }
    }
}
