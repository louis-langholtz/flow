#include <gtest/gtest.h>

#include "flow/channel.hpp"
#include "flow/instance.hpp"
#include "flow/invalid_link.hpp"
#include "flow/node.hpp"

using namespace flow;

TEST(channel, default_construction)
{
    EXPECT_NO_THROW(channel());
    EXPECT_TRUE(std::holds_alternative<reference_channel>(channel()));
}

TEST(make_channel, with_diff_sizes)
{
    using flow::link; // disambiguate link
    const auto pconns = std::vector<link>{
        link{user_endpoint{}, node_endpoint{
            {}, {reference_descriptor{1}}
        }}
    };
    EXPECT_THROW(make_channel(link{}, node_name{}, port_map{}, {}, {},
                              pconns, {}), std::logic_error);
}

TEST(make_channel, for_subsys_to_file)
{
    using flow::link; // disambiguate link
    auto chan = channel{};
    const auto name = node_name{};
    const auto sys = flow::system{
        .nodes = {
            {"subsys", flow::node{}},
        }
    };
    const auto conn = link{
        node_endpoint{"subsys"},
        file_endpoint{"file"},
    };
    const auto pconns = std::vector<link>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, port_map{}, sys, {},
                                        pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<file_channel>(chan));
}

TEST(make_channel, for_file_to_subsys)
{
    using flow::link; // disambiguate link
    auto chan = channel{};
    const auto name = node_name{};
    const auto sys = flow::system{
        .nodes = {
            {"subsys", flow::node{}},
        }
    };
    const auto conn = link{
        file_endpoint{"file"},
        node_endpoint{"subsys"},
    };
    const auto pconns = std::vector<link>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, port_map{}, sys, {},
                                        pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<file_channel>(chan));
}

TEST(make_channel, for_default_subsys_to_default_subsys)
{
    using flow::link; // disambiguate link
    auto chan = channel{};
    const auto name = node_name{};
    const auto sys = flow::system{
        .nodes = {
            {"subsys_a", flow::node{}},
            {"subsys_b", flow::node{}},
        }
    };
    const auto conn = link{
        node_endpoint{"subsys_a"},
        node_endpoint{"subsys_b"},
    };
    const auto pconns = std::vector<link>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, port_map{}, sys, {},
                                        pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<pipe_channel>(chan));
}

TEST(make_channel, for_exe_subsys_to_sys)
{
    using flow::link; // disambiguate link
    auto chan = channel{};
    const auto name = node_name{};
    const auto sys = flow::system{
        .nodes = {
            {"subsys_a", flow::executable{}},
        }
    };
    ASSERT_FALSE(empty(std_ports));
    const auto conn = link{
        node_endpoint{"subsys_a", {reference_descriptor{1}}},
        node_endpoint{{}, {reference_descriptor{1}}},
    };
    const auto pconns = std::vector<link>{
        link{user_endpoint{}, node_endpoint{
            {}, {reference_descriptor{1}}
        }}
    };
    auto pchans = std::vector<channel>{};
    pchans.emplace_back(channel{pipe_channel{}});
    EXPECT_NO_THROW(chan = make_channel(conn, name, std_ports, sys, {},
                                        pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<reference_channel>(chan));
}

TEST(make_channel, signal_channel)
{
    using flow::link; // disambiguate link
    const auto exe_name = node_name{"exe"};
    const auto sig = flow::signals::winch();
    auto name = flow::node_name{};
    auto sys = flow::system{
        .nodes = {{exe_name, {
            executable{},
            port_map{
                {sig, {"", io_type::in}},
                stdout_ports_entry,
                stderr_ports_entry
            }
        }}}
    };
    const auto interface = port_map{{sig, {"", io_type::in}}};
    auto conn = flow::link{
        node_endpoint{{}, {sig}},
        node_endpoint{exe_name, {sig}},
    };
    auto channels = std::vector<channel>{};
    auto pconns = std::vector<link>{};
    auto pchans = std::vector<channel>{};
    const auto rv = make_channel(conn, name, interface, sys, channels,
                                 pconns, pchans);
    ASSERT_TRUE(std::holds_alternative<signal_channel>(rv));
    const auto& sc = std::get<signal_channel>(rv);
    EXPECT_EQ(sc.address, exe_name);
    ASSERT_EQ(size(sc.signals), 1u);
    EXPECT_EQ(*sc.signals.begin(), sig);
}
