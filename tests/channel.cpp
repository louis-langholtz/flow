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

TEST(make_channel, with_defaulted_args)
{
    using flow::link; // disambiguate link
    EXPECT_THROW(make_channel(link{}, node_name{}, flow::node{}, {},
                              {}, {}), invalid_link);
}

TEST(make_channel, with_diff_sizes)
{
    using flow::link; // disambiguate link
    const auto pconns = std::vector<link>{
        unidirectional_link{user_endpoint{}, node_endpoint{
            {}, {reference_descriptor{1}}
        }}
    };
    EXPECT_THROW(make_channel(link{}, node_name{}, flow::node{}, {},
                              pconns, {}), std::logic_error);
}

TEST(make_channel, same_ends)
{
    const auto the_end = node_endpoint{"a"};
    try {
        make_channel(unidirectional_link{the_end, the_end}, node_name{},
                     flow::node{}, {}, {}, {});
        FAIL() << "expected invalid_link, got none";
    }
    catch (const invalid_link& ex) {
        EXPECT_EQ(ex.value, flow::link(unidirectional_link{the_end, the_end}));
        EXPECT_EQ(std::string(ex.what()), "must have different endpoints");
    }
    catch (...) {
        FAIL() << "expected invalid_link, got other exception";
    }
}

TEST(make_channel, end_to_nonesuch)
{
    const auto the_link = flow::link{unidirectional_link{
        node_endpoint{node_name{}},
        node_endpoint{node_name{"b"}},
    }};
    try {
        make_channel(the_link, node_name{}, flow::node{}, {}, {}, {});
        FAIL() << "expected invalid_link, got none";
    }
    catch (const invalid_link& ex) {
        EXPECT_EQ(ex.value, the_link);
        EXPECT_EQ(std::string(ex.what()), "endpoint addressed node b not found");
    }
    catch (...) {
        FAIL() << "expected invalid_link, got other exception";
    }
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
    const auto conn = unidirectional_link{
        node_endpoint{"subsys"},
        file_endpoint{"file"},
    };
    const auto pconns = std::vector<link>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, sys, {}, pconns, pchans));
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
    const auto conn = unidirectional_link{
        file_endpoint{"file"},
        node_endpoint{"subsys"},
    };
    const auto pconns = std::vector<link>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, sys, {}, pconns, pchans));
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
    const auto conn = unidirectional_link{
        node_endpoint{"subsys_a"},
        node_endpoint{"subsys_b"},
    };
    const auto pconns = std::vector<link>{};
    auto pchans = std::vector<channel>{};
    EXPECT_NO_THROW(chan = make_channel(conn, name, sys, {}, pconns, pchans));
    EXPECT_TRUE(std::holds_alternative<pipe_channel>(chan));
}

TEST(make_channel, for_exe_subsys_to_sys)
{
    using flow::link; // disambiguate link
    auto chan = channel{};
    const auto name = node_name{};
    const auto sys = flow::node{
        flow::system{
            .nodes = {
                {"subsys_a", flow::executable{}},
            }
        }, std_ports,
    };
    ASSERT_FALSE(empty(sys.interface));
    const auto conn = unidirectional_link{
        node_endpoint{"subsys_a", {reference_descriptor{1}}},
        node_endpoint{{}, {reference_descriptor{1}}},
    };
    const auto pconns = std::vector<link>{
        unidirectional_link{user_endpoint{}, node_endpoint{
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
    using flow::link; // disambiguate link
    const auto exe_name = node_name{"exe"};
    const auto sig = flow::signals::winch();
    auto name = flow::node_name{};
    auto sys = flow::node{
        flow::system{
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
    auto conn = flow::unidirectional_link{
        node_endpoint{{}, {sig}},
        node_endpoint{exe_name, {sig}},
    };
    auto channels = std::vector<channel>{};
    auto pconns = std::vector<link>{};
    auto pchans = std::vector<channel>{};
    const auto rv = make_channel(conn, name, sys, channels, pconns, pchans);
    ASSERT_TRUE(std::holds_alternative<signal_channel>(rv));
    const auto& sc = std::get<signal_channel>(rv);
    EXPECT_EQ(sc.address, exe_name);
    ASSERT_EQ(size(sc.signals), 1u);
    EXPECT_EQ(*sc.signals.begin(), sig);
}
