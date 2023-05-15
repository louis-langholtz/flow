#include <gtest/gtest.h>

#include "flow/env_name.hpp"

TEST(env_name, default_construction)
{
    EXPECT_NO_THROW(flow::env_name());
    EXPECT_TRUE(flow::env_name().get().empty());
}

TEST(env_name, construction)
{
    EXPECT_NO_THROW(flow::env_name());
    EXPECT_NO_THROW(flow::env_name("PATH"));
    EXPECT_NO_THROW(flow::env_name("HOME"));

    EXPECT_THROW(flow::env_name("=FOO"), std::invalid_argument);
    EXPECT_THROW(flow::env_name("FOO="), std::invalid_argument);
    EXPECT_THROW(flow::env_name("=BAR"), std::invalid_argument);
    EXPECT_THROW(flow::env_name("BAR="), std::invalid_argument);
    EXPECT_THROW(flow::env_name("FOO=BAR"), std::invalid_argument);
    EXPECT_THROW(flow::env_name("="), std::invalid_argument);
    EXPECT_THROW(flow::env_name("=="), std::invalid_argument);

    EXPECT_THROW(flow::env_name(std::string{'\0'}),
                 std::invalid_argument);
    EXPECT_THROW(flow::env_name(std::string{'\0', '\0'}),
                 std::invalid_argument);
    EXPECT_THROW(flow::env_name(std::string{'A', '\0'}),
                 std::invalid_argument);
    EXPECT_THROW(flow::env_name(std::string{'\0', 'c'}),
                 std::invalid_argument);
    EXPECT_THROW(flow::env_name(std::string{'a', '\0', 'b'}),
                 std::invalid_argument);
    EXPECT_THROW(flow::env_name(std::string{'a', '=', 'b'}),
                 std::invalid_argument);
    EXPECT_THROW(flow::env_name(std::string{'a', '=', '\0', 'b'}),
                 std::invalid_argument);
}
