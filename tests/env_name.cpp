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

    EXPECT_THROW(flow::env_name("=FOO"), flow::charset_validator_error);
    EXPECT_THROW(flow::env_name("FOO="), flow::charset_validator_error);
    EXPECT_THROW(flow::env_name("=BAR"), flow::charset_validator_error);
    EXPECT_THROW(flow::env_name("BAR="), flow::charset_validator_error);
    EXPECT_THROW(flow::env_name("FOO=BAR"), flow::charset_validator_error);
    EXPECT_THROW(flow::env_name("="), flow::charset_validator_error);
    EXPECT_THROW(flow::env_name("=="), flow::charset_validator_error);

    EXPECT_THROW(flow::env_name(std::string{'\0'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_name(std::string{'\0', '\0'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_name(std::string{'A', '\0'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_name(std::string{'\0', 'c'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_name(std::string{'a', '\0', 'b'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_name(std::string{'a', '=', 'b'}),
                 flow::charset_validator_error);

    const auto good_string = std::string{'a', 'b', 'c'};
    const auto bad_string = std::string{'a', '=', '\0', 'b'};
    EXPECT_NO_THROW(flow::env_name(good_string.begin(), good_string.end()));
    EXPECT_THROW(flow::env_name(bad_string.begin(), bad_string.end()),
                 flow::charset_validator_error);
}
