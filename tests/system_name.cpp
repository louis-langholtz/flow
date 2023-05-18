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
    EXPECT_NO_THROW(flow::system_name("some_name-to-test-5"));
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
    EXPECT_NO_THROW(flow::system_name(std::string{'-'}));
    EXPECT_NO_THROW(flow::system_name("system#33"));

    EXPECT_THROW(flow::system_name(std::string{'\0'}), std::invalid_argument);
    EXPECT_THROW(flow::system_name("."), std::invalid_argument);
    EXPECT_THROW(flow::system_name("system@"), std::invalid_argument);
    EXPECT_THROW(flow::system_name("system:0"), std::invalid_argument);
}
