#include <gtest/gtest.h>

#include <numeric> // for std::iota
#include <vector>

#include "flow/charset_checker.hpp"

using namespace flow;
using namespace flow::detail;

TEST(charset_checker, default_construction)
{
    EXPECT_NO_THROW(charset_checker<char_list::deny>());
    EXPECT_NO_THROW(charset_checker<char_list::allow>());
    EXPECT_NO_THROW((charset_checker<char_list::deny, name_charset>()));
    EXPECT_NO_THROW((charset_checker<char_list::allow, name_charset>()));
}

TEST(charset_checker, charset)
{
    auto upper_chars = std::string(26u, ' ');
    std::iota(begin(upper_chars), end(upper_chars), 'A');
    auto lower_chars = std::string(26u, ' ');
    std::iota(begin(lower_chars), end(lower_chars), 'a');
    const auto expected = upper_chars + lower_chars;
    EXPECT_EQ((charset_checker<char_list::deny,
               upper_charset, lower_charset>::charset),
              expected);
    EXPECT_EQ((charset_checker<char_list::deny,
               upper_charset, upper_charset, lower_charset>::charset),
              expected);
}

TEST(denied_chars_checker, check)
{
    EXPECT_THROW(denied_chars_checker<upper_charset>()("A"),
                 charset_validator_error);
    EXPECT_THROW(denied_chars_checker<lower_charset>()("a"),
                 charset_validator_error);
    EXPECT_NO_THROW(denied_chars_checker<upper_charset>()("a"));
    EXPECT_NO_THROW(denied_chars_checker<lower_charset>()("A"));
}

TEST(denied_chars_checker, exception)
{
    using charset = upper_charset;
    try {
        denied_chars_checker<charset>()("01234A");
        FAIL() << "expected exception";
    }
    catch (const charset_validator_error& ex) {
        EXPECT_EQ(ex.badchar(), 'A');
        EXPECT_EQ(ex.access(), char_list::deny);
        EXPECT_EQ(ex.charset(), std::string_view(charset()));
    }
    catch (...) {
        FAIL() << "caught unexpected exception type";
    }
}
