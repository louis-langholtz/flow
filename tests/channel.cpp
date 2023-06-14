#include <gtest/gtest.h>

#include "flow/channel.hpp"
#include "flow/instance.hpp"
#include "flow/system.hpp"

using namespace flow;

TEST(channel, default_construction)
{
    EXPECT_NO_THROW(channel());
    EXPECT_TRUE(std::holds_alternative<reference_channel>(channel()));
}

TEST(make_channel, with_defaulted_args)
{
    EXPECT_THROW(make_channel(connection{}, system_name{}, flow::system{}, {},
                              {}, {}), invalid_connection);
}

TEST(make_channel, with_diff_system_endpoints)
{
    const auto conn = unidirectional_connection{
        system_endpoint{"a"},
        system_endpoint{"b"},
    };
    EXPECT_THROW(make_channel(conn, system_name{}, flow::system{}, {},
                              {}, {}), invalid_connection);
}

TEST(make_channel, with_diff_sizes)
{
    const auto pconns = std::vector<connection>{
        unidirectional_connection{user_endpoint{}, system_endpoint{
            {}, {reference_descriptor{1}}
        }}
    };
    EXPECT_THROW(make_channel(connection{}, system_name{}, flow::system{}, {},
                              pconns, {}), std::logic_error);
}

TEST(make_channel, for_subsys_to_file)
{
    auto chan = channel{};
    const auto name = system_name{};
    const auto sys = flow::custom{
        .nodes = {
            {"subsys", flow::system{}},
        }
    };
    const auto conn = unidirectional_connection{
        system_endpoint{"subsys"},
        file_endpoint{"file"},
    };
    const auto pconns = std::vector<connection>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, sys, {}, pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<file_channel>(chan));
}

TEST(make_channel, for_file_to_subsys)
{
    auto chan = channel{};
    const auto name = system_name{};
    const auto sys = flow::custom{
        .nodes = {
            {"subsys", flow::system{}},
        }
    };
    const auto conn = unidirectional_connection{
        file_endpoint{"file"},
        system_endpoint{"subsys"},
    };
    const auto pconns = std::vector<connection>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, sys, {}, pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<file_channel>(chan));
}

TEST(make_channel, for_default_subsys_to_default_subsys)
{
    auto chan = channel{};
    const auto name = system_name{};
    const auto sys = flow::custom{
        .nodes = {
            {"subsys_a", flow::system{}},
            {"subsys_b", flow::system{}},
        }
    };
    const auto conn = unidirectional_connection{
        system_endpoint{"subsys_a"},
        system_endpoint{"subsys_b"},
    };
    const auto pconns = std::vector<connection>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, sys, {}, pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<pipe_channel>(chan));
}

TEST(make_channel, for_exe_subsys_to_sys)
{
    auto chan = channel{};
    const auto name = system_name{};
    const auto sys = flow::system{
        flow::custom{
            .nodes = {
                {"subsys_a", flow::executable{}},
            }
        }, std_ports,
    };
    ASSERT_FALSE(empty(sys.interface));
    const auto conn = unidirectional_connection{
        system_endpoint{"subsys_a", {reference_descriptor{1}}},
        system_endpoint{{}, {reference_descriptor{1}}},
    };
    const auto pconns = std::vector<connection>{
        unidirectional_connection{user_endpoint{}, system_endpoint{
            {}, {reference_descriptor{1}}
        }}
    };
    auto pchans = std::vector<channel>{};
    pchans.emplace_back(channel{pipe_channel{}});
    EXPECT_NO_THROW(chan = make_channel(conn, name, sys, {}, pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<reference_channel>(chan));
}

TEST(make_channel, signal_channel)
{
    const auto exe_name = system_name{"exe"};
    const auto sig = flow::signals::winch();
    auto name = flow::system_name{};
    auto sys = flow::system{
        custom{
            .nodes = {{exe_name, {
                executable{},
                port_map{
                    {sig, {"", io_type::in}},
                    stdout_ports_entry,
                    stderr_ports_entry
                }
            }}}
        },
        port_map{{sig, {"", io_type::in}}}
    };
    auto conn = flow::unidirectional_connection{
        system_endpoint{{}, {sig}},
        system_endpoint{exe_name, {sig}},
    };
    auto channels = std::vector<channel>{};
    auto pconns = std::vector<connection>{};
    auto pchans = std::vector<channel>{};
    const auto rv = make_channel(conn, name, sys, channels, pconns, pchans);
    ASSERT_TRUE(std::holds_alternative<signal_channel>(rv));
    const auto& sc = std::get<signal_channel>(rv);
    EXPECT_EQ(sc.address, exe_name);
    ASSERT_EQ(size(sc.signals), 1u);
    EXPECT_EQ(*sc.signals.begin(), sig);
}
