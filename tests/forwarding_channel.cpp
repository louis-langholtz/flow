#include <system_error>

#include <gtest/gtest.h>

#include <fcntl.h> // for ::open

#include "flow/pipe_channel.hpp"
#include "flow/forwarding_channel.hpp"

using namespace flow;

TEST(forwarding_channel, default_construction)
{
    EXPECT_NO_THROW(forwarding_channel());
    EXPECT_EQ(forwarding_channel().source(), descriptors::invalid_id);
    EXPECT_EQ(forwarding_channel().destination(), descriptors::invalid_id);
    EXPECT_FALSE(forwarding_channel().valid());
}

TEST(forwarding_channel, invalid_invalid)
{
    ASSERT_NO_THROW(forwarding_channel(descriptors::invalid_id,
                                       descriptors::invalid_id));
    auto obj = forwarding_channel(descriptors::invalid_id,
                                  descriptors::invalid_id);
    EXPECT_EQ(obj.source(), descriptors::invalid_id);
    EXPECT_EQ(obj.destination(), descriptors::invalid_id);
    EXPECT_TRUE(obj.valid());
    EXPECT_THROW(obj.get_result(), std::system_error);
}

TEST(forwarding_channel, dev_null)
{
    auto src_d = owning_descriptor{::open("/dev/null", O_RDONLY, 0600)};
    auto dst_d = owning_descriptor{::open("/dev/null", O_WRONLY, 0600)};
    auto obj = forwarding_channel(std::move(src_d), std::move(dst_d));
    EXPECT_NE(obj.source(), descriptors::invalid_id);
    EXPECT_NE(obj.destination(), descriptors::invalid_id);
    EXPECT_TRUE(obj.valid());
    auto counters = forwarding_channel::counters{};
    EXPECT_NO_THROW(counters = obj.get_result());
    EXPECT_EQ(counters.reads, 1u);
    EXPECT_EQ(counters.writes, 0u);
}

TEST(forwarding_channel, pipe_channels)
{
    auto in = pipe_channel{};
    auto out = pipe_channel{};
    auto obj = forwarding_channel{
        in.get(pipe_channel::io::read),
        out.get(pipe_channel::io::write)
    };
    EXPECT_EQ(obj.source(), in.get(pipe_channel::io::read));
    EXPECT_EQ(obj.destination(), out.get(pipe_channel::io::write));
    EXPECT_TRUE(obj.valid());
    constexpr char text[] = "hello world!";
    in.write(text, std::cerr);
    in.close(pipe_channel::io::write, std::cerr);
    auto counters = forwarding_channel::counters{};
    EXPECT_NO_THROW(counters = obj.get_result());
    EXPECT_EQ(counters.reads, 2u);
    EXPECT_EQ(counters.writes, 1u);
    EXPECT_EQ(counters.bytes, std::size(text));
    auto buffer = std::array<char, 128>{};
    const auto nread = out.read(buffer, std::cerr);
    out.close();
    EXPECT_EQ(nread, std::size(text));
}
