#include <gtest/gtest.h>

#include "ext/fstream.hpp"

TEST(fstream, default_construction)
{
    ext::fstream obj;
    EXPECT_FALSE(obj.is_open());
    EXPECT_TRUE(obj.good());
    EXPECT_NE(obj.rdbuf(), nullptr);
    const auto filebuf = dynamic_cast<ext::filebuf *>(obj.rdbuf());
    EXPECT_NE(filebuf, nullptr);
}

TEST(fstream, temporary_fstream)
{
    ext::fstream obj;
    EXPECT_NO_THROW(obj = ext::temporary_fstream());
    EXPECT_TRUE(obj.is_open());
    EXPECT_TRUE(obj.good());
    EXPECT_EQ(obj.tellg(), 0);
    EXPECT_EQ(obj.tellp(), 0);
}
