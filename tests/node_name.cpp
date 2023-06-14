#include <gtest/gtest.h>

#include "flow/node_name.hpp"

using namespace flow;

TEST(node_name, default_construction)
{
    EXPECT_NO_THROW(node_name());
    EXPECT_TRUE(node_name().get().empty());
}

TEST(node_name, construction)
{
    EXPECT_NO_THROW(node_name());
    EXPECT_NO_THROW(node_name("_some_name_to_test_5"));
    for (auto c = 'A'; c <= 'Z'; ++c) {
        EXPECT_NO_THROW(node_name(std::string{c}));
    }
    for (auto c = 'a'; c <= 'z'; ++c) {
        EXPECT_NO_THROW(node_name(std::string{c}));
    }
    for (auto c = '0'; c <= '9'; ++c) {
        EXPECT_NO_THROW(node_name(std::string{c}));
    }
    EXPECT_NO_THROW(node_name(std::string{'_'}));
    EXPECT_NO_THROW(node_name("system_33"));

    EXPECT_THROW(node_name(std::string{'\0'}), charset_validator_error);
    EXPECT_THROW(node_name("-"), charset_validator_error);
    EXPECT_THROW(node_name("."), charset_validator_error);
    EXPECT_THROW(node_name("system@"), charset_validator_error);
    EXPECT_THROW(node_name("system:0"), charset_validator_error);
    EXPECT_THROW(node_name("system#33"), charset_validator_error);
}

TEST(node_name, ostream_operator_support)
{
    const auto name = std::string{"test"};
    std::stringstream os;
    os << node_name{name};
    const auto got = os.str();
    EXPECT_EQ(got, name);
}

TEST(node_name, ranged_ostream_operator_support)
{
    std::stringstream os;
    const auto names = std::vector<node_name>{"test", "one", "two"};
    os << names;
    const auto got = os.str();
    EXPECT_EQ(got, std::string("test.one.two"));
}

TEST(to_system_names, with_empty_string)
{
    auto result = decltype(to_system_names(std::string{})){};
    EXPECT_NO_THROW(result = to_system_names(std::string{}));
    EXPECT_TRUE(empty(result));
}

TEST(to_system_names, with_good_strings)
{
    auto result = decltype(to_system_names(std::string{})){};
    EXPECT_NO_THROW(result = to_system_names(""));
    EXPECT_TRUE(empty(result));
    EXPECT_NO_THROW(result = to_system_names("."));
    EXPECT_EQ(size(result), 2u);
    if (size(result) == 2u) {
        EXPECT_EQ(result[0], node_name(""));
        EXPECT_EQ(result[1], node_name(""));
    }
    EXPECT_NO_THROW(result = to_system_names("a_system"));
    EXPECT_EQ(size(result), 1u);
    if (size(result) == 1u) {
        EXPECT_EQ(result[0], node_name("a_system"));
    }
    EXPECT_NO_THROW(result = to_system_names(".a_system"));
    EXPECT_EQ(size(result), 2u);
    if (size(result) == 2u) {
        EXPECT_EQ(result[0], node_name(""));
        EXPECT_EQ(result[1], node_name("a_system"));
    }
    EXPECT_NO_THROW(result = to_system_names("a.b.c"));
    EXPECT_EQ(size(result), 3u);
    if (size(result) == 3u) {
        EXPECT_EQ(result[0], node_name("a"));
        EXPECT_EQ(result[1], node_name("b"));
        EXPECT_EQ(result[2], node_name("c"));
    }
}

TEST(to_system_names, with_bad_strings)
{
    EXPECT_THROW(to_system_names(":a.b.c"), charset_validator_error);
    EXPECT_THROW(to_system_names("@b-c"), charset_validator_error);
    EXPECT_THROW(to_system_names("'"), charset_validator_error);
    EXPECT_THROW(to_system_names(":"), charset_validator_error);
}
