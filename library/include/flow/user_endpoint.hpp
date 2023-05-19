#ifndef user_endpoint_hpp
#define user_endpoint_hpp

#include <ostream>
#include <string>

#include "flow/checked.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct user_endpoint
{
    using name_charset_checker = detail::allowed_chars_checker<
        detail::name_charset, detail::tcstring<'+',':','.'>
    >;

    detail::checked<std::string, name_charset_checker, user_endpoint> name;

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

}

#endif /* user_endpoint_hpp */
