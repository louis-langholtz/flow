#include "flow/user_endpoint.hpp"

namespace flow {

namespace {
constexpr auto prefix_char = '^';
}

auto operator<<(std::ostream& os, const user_endpoint& value)
    -> std::ostream&
{
    os << prefix_char;
    os << value.name;
    return os;
}

auto operator>>(std::istream& is, user_endpoint& value) -> std::istream&
{
    if (is.peek() != prefix_char) {
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
