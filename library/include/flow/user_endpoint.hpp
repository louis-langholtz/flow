#ifndef user_endpoint_hpp
#define user_endpoint_hpp

#include <istream>
#include <ostream>
#include <string>

#include "flow/checked.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct user_endpoint
{
    using name_type = detail::checked<
        std::string,
        detail::allowed_chars_checker<
            detail::name_charset, detail::tcstring<'+',':','.'>
        >, user_endpoint
    >;

    name_type name;

    auto operator==(const user_endpoint& other) const noexcept
    {
        return name == other.name;
    }

    auto operator<(const user_endpoint& other) const noexcept
    {
        return name < other.name;
    }
};

auto operator<<(std::ostream& os, const user_endpoint& value)
    -> std::ostream&;

auto operator>>(std::istream& is, user_endpoint& value) -> std::istream&;

}

#endif /* user_endpoint_hpp */
