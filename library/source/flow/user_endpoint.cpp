#include "flow/reserved.hpp"
#include "flow/user_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const user_endpoint& value)
    -> std::ostream&
{
    os << reserved::user_endpoint_prefix;
    os << value.name;
    return os;
}

auto operator>>(std::istream& is, user_endpoint& value) -> std::istream&
{
    if (is.peek() != reserved::user_endpoint_prefix) {
        is.setstate(std::ios::failbit);
        return is;
    }
    auto string = std::string{};
    auto c = char{};
    is >> c; // skip the prefix char
    is >> string;
    value = user_endpoint{string};
    return is;
}

}
