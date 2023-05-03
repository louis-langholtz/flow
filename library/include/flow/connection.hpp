#ifndef connection_hpp
#define connection_hpp

#include <array>
#include <ostream>

#include "flow/endpoint.hpp"
#include "flow/variant.hpp"

namespace flow {

struct unidirectional_connection {
    endpoint src;
    endpoint dst;
};

constexpr auto operator==(const unidirectional_connection& lhs,
                          const unidirectional_connection& rhs) noexcept
    -> bool
{
    return (lhs.src == rhs.src) && (lhs.dst == rhs.dst);
}

auto operator<<(std::ostream& os, const unidirectional_connection& value)
    -> std::ostream&;

struct bidirectional_connection {
    std::array<endpoint, 2u> ends;
};

constexpr auto operator==(const bidirectional_connection& lhs,
                          const bidirectional_connection& rhs) noexcept
    -> bool
{
    return lhs.ends == rhs.ends;
}

auto operator<<(std::ostream& os, const bidirectional_connection& value)
    -> std::ostream&;

using connection = variant<
    unidirectional_connection,
    bidirectional_connection
>;

}

#endif /* connection_hpp */
