#ifndef user_endpoint_hpp
#define user_endpoint_hpp

#include <compare>
#include <ostream>
#include <string>

#include "flow/checked_value.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct user_endpoint
{
    /// @brief Characters checker.
    /// @note Basically <code>detail::name_chars_checker</code> plus
    ///   colon and period.
    using chars_checker = detail::allowed_chars_checker<
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        'a','b','c','d','e','f','g','h','i','j','k','l','m',
        'n','o','p','q','r','s','t','u','v','w','x','y','z',
        '0','1','2','3','4','5','6','7','8','9','-','_','+',
        ':','.'
    >;

    struct name_checker: chars_checker {};

    detail::checked_value<std::string, name_checker> name;

#if defined(__cpp_lib_three_way_comparison)
    auto operator<=>(const user_endpoint&) const = default;
#else
    auto operator==(const user_endpoint& other) const noexcept
    {
        return name == other.name;
    }

    auto operator<(const user_endpoint& other) const noexcept
    {
        return name < other.name;
    }
#endif
};

auto operator<<(std::ostream& os, const user_endpoint& value)
    -> std::ostream&;

}

#endif /* user_endpoint_hpp */
