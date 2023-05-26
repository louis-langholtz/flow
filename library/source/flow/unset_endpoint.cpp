#include "flow/reserved.hpp"
#include "flow/unset_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const unset_endpoint&) -> std::ostream&
{
    os << reserved::unset_endpoint_prefix;
    return os;
}

auto operator>>(std::istream& is, unset_endpoint&) -> std::istream&
{
    if (is.peek() != reserved::unset_endpoint_prefix) {
        is.setstate(std::ios::failbit);
        return is;
    }
    auto c = char{};
    is >> c; // skip the prefix char
    return is;
}

}
