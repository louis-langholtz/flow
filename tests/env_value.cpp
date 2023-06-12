#include <gtest/gtest.h>

#include "flow/env_value.hpp"

TEST(env_value, default_construction)
{
    EXPECT_NO_THROW(flow::env_value());
    EXPECT_TRUE(flow::env_value().get().empty());
}

TEST(env_value, construction)
{
    EXPECT_NO_THROW(flow::env_value());
    EXPECT_NO_THROW(flow::env_value("/some/path:/or/other"));
    EXPECT_NO_THROW(flow::env_value("0101"));
    EXPECT_NO_THROW(flow::env_value("OK="));
    EXPECT_NO_THROW(flow::env_value("=OK"));

    EXPECT_THROW(flow::env_value(std::string{'\0'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_value(std::string{'\0', '\0'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_value(std::string{'A', '\0'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_value(std::string{'\0', 'c'}),
                 flow::charset_validator_error);
    EXPECT_THROW(flow::env_value(std::string{'a', '\0', 'b'}),
                 flow::charset_validator_error);
}
