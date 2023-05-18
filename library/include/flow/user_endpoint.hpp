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
    struct name_checker: detail::allowed_chars_checker<
        detail::name_charset, detail::constexpr_ntsb<'+',':','.'>
    > {};

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
