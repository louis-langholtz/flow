#include "flow/unset_endpoint.hpp"

namespace flow {

namespace {
constexpr auto prefix_char = '-';
}

auto operator<<(std::ostream& os, const unset_endpoint&) -> std::ostream&
{
    os << prefix_char;
    return os;
}

auto operator>>(std::istream& is, unset_endpoint&) -> std::istream&
{
    if (is.peek() != prefix_char) {
        is.setstate(std::ios::failbit);
        return is;
    }
    auto c = char{};
    is >> c; // skip the prefix char
    return is;
}

}
