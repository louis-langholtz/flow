#include <gtest/gtest.h>

#include "flow/system_name.hpp"

TEST(system_name, default_construction)
{
    EXPECT_NO_THROW(flow::system_name());
    EXPECT_TRUE(flow::system_name().get().empty());
}

TEST(system_name, construction)
{
    EXPECT_NO_THROW(flow::system_name());
    EXPECT_NO_THROW(flow::system_name("_some_name_to_test_5"));
    for (auto c = 'A'; c <= 'Z'; ++c) {
        EXPECT_NO_THROW(flow::system_name(std::string{c}));
    }
    for (auto c = 'a'; c <= 'z'; ++c) {
        EXPECT_NO_THROW(flow::system_name(std::string{c}));
    }
    for (auto c = '0'; c <= '9'; ++c) {
        EXPECT_NO_THROW(flow::system_name(std::string{c}));
    }
    EXPECT_NO_THROW(flow::system_name(std::string{'_'}));
    EXPECT_NO_THROW(flow::system_name("system#33"));

    EXPECT_THROW(flow::system_name(std::string{'\0'}), std::invalid_argument);
    EXPECT_THROW(flow::system_name("-"), std::invalid_argument);
    EXPECT_THROW(flow::system_name("."), std::invalid_argument);
    EXPECT_THROW(flow::system_name("system@"), std::invalid_argument);
    EXPECT_THROW(flow::system_name("system:0"), std::invalid_argument);
}

TEST(system_name, ostream_operator_support)
{
    const auto name = std::string{"test"};
    std::stringstream os;
    os << flow::system_name{name};
    const auto got = os.str();
    EXPECT_EQ(got, name);
}

TEST(system_name, ranged_ostream_operator_support)
{
    std::stringstream os;
    const auto names = std::vector<flow::system_name>{"test", "one", "two"};
    os << names;
    const auto got = os.str();
    EXPECT_EQ(got, std::string("test.one.two"));
}

TEST(to_system_names, with_empty_string)
{
    auto result = decltype(flow::to_system_names(std::string{})){};
    EXPECT_NO_THROW(result = flow::to_system_names(std::string{}));
    EXPECT_TRUE(empty(result));
}

TEST(to_system_names, with_good_strings)
{
    auto result = decltype(flow::to_system_names(std::string{})){};
    EXPECT_NO_THROW(result = flow::to_system_names(""));
    EXPECT_TRUE(empty(result));
    EXPECT_NO_THROW(result = flow::to_system_names("."));
    EXPECT_EQ(size(result), 2u);
    if (size(result) == 2u) {
        EXPECT_EQ(result[0], flow::system_name(""));
        EXPECT_EQ(result[1], flow::system_name(""));
    }
    EXPECT_NO_THROW(result = flow::to_system_names("a_system"));
    EXPECT_EQ(size(result), 1u);
    if (size(result) == 1u) {
        EXPECT_EQ(result[0], flow::system_name("a_system"));
    }
    EXPECT_NO_THROW(result = flow::to_system_names(".a_system"));
    EXPECT_EQ(size(result), 2u);
    if (size(result) == 2u) {
        EXPECT_EQ(result[0], flow::system_name(""));
        EXPECT_EQ(result[1], flow::system_name("a_system"));
    }
    EXPECT_NO_THROW(result = flow::to_system_names("a.b.c"));
    EXPECT_EQ(size(result), 3u);
    if (size(result) == 3u) {
        EXPECT_EQ(result[0], flow::system_name("a"));
        EXPECT_EQ(result[1], flow::system_name("b"));
        EXPECT_EQ(result[2], flow::system_name("c"));
    }
}

TEST(to_system_names, with_bad_strings)
{
    EXPECT_THROW(flow::to_system_names(":a.b.c"), std::invalid_argument);
    EXPECT_THROW(flow::to_system_names("@b-c"), std::invalid_argument);
    EXPECT_THROW(flow::to_system_names("'"), std::invalid_argument);
    EXPECT_THROW(flow::to_system_names(":"), std::invalid_argument);
}
