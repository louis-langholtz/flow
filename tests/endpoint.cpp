#include <sstream> // for std::stringstream

#include <gtest/gtest.h>

#include "flow/endpoint.hpp"

using namespace flow;

TEST(endpoint, default_construction)
{
    EXPECT_NO_THROW(endpoint());
    EXPECT_TRUE(std::holds_alternative<unset_endpoint>(endpoint()));
}

TEST(endpoint, stream_roundtrip_file)
{
    auto result = endpoint{};
    std::stringstream ss;
    const auto from = endpoint(file_endpoint{"/bin/false"});
    EXPECT_NO_THROW(ss << from);
    EXPECT_NO_THROW(ss >> result);
    EXPECT_FALSE(ss.fail());
    EXPECT_TRUE(std::holds_alternative<file_endpoint>(result));
    EXPECT_EQ(result, from);
}
