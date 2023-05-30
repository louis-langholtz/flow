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
    const auto sys = flow::system::custom{
        .subsystems = {
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
    const auto sys = flow::system::custom{
        .subsystems = {
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
    const auto sys = flow::system::custom{
        .subsystems = {
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
        flow::system::custom{
            .subsystems = {
                {"subsys_a", flow::system::executable{}},
            }
        }, std_ports,
    };
    ASSERT_FALSE(empty(sys.ports));
    const auto conn = unidirectional_connection{
        system_endpoint{"subsys_a"},
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
