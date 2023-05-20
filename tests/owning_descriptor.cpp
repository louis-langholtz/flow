#include <gtest/gtest.h>
#include <fcntl.h> // for open

#include "flow/owning_descriptor.hpp"

using namespace flow;

TEST(owning_descriptor, default_construction)
{
    ASSERT_NO_THROW(owning_descriptor());
    EXPECT_EQ(int(owning_descriptor::default_descriptor), -1);
    EXPECT_EQ(reference_descriptor(owning_descriptor()),
              owning_descriptor::default_descriptor);
}

TEST(owning_descriptor, open)
{
    owning_descriptor d{::open("/", O_RDONLY, 0)};
    EXPECT_NE(int(d), -1);
    EXPECT_EQ(d.close(), os_error_code(0));
    EXPECT_EQ(int(d), -1);
}

TEST(owning_descriptor, move_construction)
{
    owning_descriptor d{::open("/", O_RDONLY, 0)};
    EXPECT_NE(int(d), -1);
    const owning_descriptor e{std::move(d)};
    EXPECT_EQ(int(d), -1);
    EXPECT_NE(int(e), -1);
}
