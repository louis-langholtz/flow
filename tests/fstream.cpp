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

TEST(fstream, unique_filesytem)
{
    const auto base_path = std::filesystem::path{"/tmp/foo.txt"};
    auto path = base_path;
    auto during_status = std::filesystem::file_status{};
    {
        ext::fstream obj;
        EXPECT_FALSE(obj.is_open());
        obj.unique(path);
        EXPECT_TRUE(obj.is_open());
        EXPECT_TRUE(obj.good());
        EXPECT_NE(path, base_path);
        EXPECT_TRUE(path.native().starts_with("/tmp/foo"));
        EXPECT_TRUE(path.native().ends_with(".txt"));
        during_status = status(path);
        EXPECT_EQ(during_status.type(), std::filesystem::file_type::regular);
        const auto min_expected_perms = std::filesystem::perms::owner_read
                                       |std::filesystem::perms::owner_write;
        EXPECT_NE((during_status.permissions() & min_expected_perms),
                  std::filesystem::perms::none);
        EXPECT_EQ(file_size(path), 0u);
        obj << "Hello world!\n";
        EXPECT_TRUE(obj.good());
    }
    const auto after_status = status(path);
    EXPECT_EQ(during_status.type(), after_status.type());
    EXPECT_EQ(during_status.permissions(), after_status.permissions());
    EXPECT_GT(file_size(path), 0u);
}

TEST(fstream, unique_string)
{
    const auto base_path = std::string{"/tmp/foo.txt"};
    auto path = base_path;
    auto during_status = std::filesystem::file_status{};
    {
        ext::fstream obj;
        EXPECT_FALSE(obj.is_open());
        obj.unique(path);
        EXPECT_TRUE(obj.is_open());
        EXPECT_TRUE(obj.good());
        EXPECT_NE(path, base_path);
        EXPECT_TRUE(path.starts_with("/tmp/foo"));
        EXPECT_TRUE(path.ends_with(".txt"));
        during_status = status(std::filesystem::path{path});
        EXPECT_EQ(during_status.type(), std::filesystem::file_type::regular);
        const auto min_expected_perms = std::filesystem::perms::owner_read
                                       |std::filesystem::perms::owner_write;
        EXPECT_NE((during_status.permissions() & min_expected_perms),
                  std::filesystem::perms::none);
        EXPECT_EQ(file_size(std::filesystem::path{path}), 0u);
        obj << "Hello world!\n";
        EXPECT_TRUE(obj.good());
    }
    const auto after_status = status(std::filesystem::path{path});
    EXPECT_EQ(during_status.type(), after_status.type());
    EXPECT_EQ(during_status.permissions(), after_status.permissions());
    EXPECT_GT(file_size(std::filesystem::path{path}), 0u);
}
