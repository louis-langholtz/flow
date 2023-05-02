#ifndef connection_hpp
#define connection_hpp

#include <array>
#include <ostream>

#include "flow/port.hpp"

namespace flow {

struct connection {
    std::array<port, 2u> ends;
};

constexpr auto operator==(const connection& lhs,
                          const connection& rhs) noexcept -> bool
{
    return lhs.ends == rhs.ends;
}

auto operator<<(std::ostream& os, const connection& value)
    -> std::ostream&;
}

#endif /* connection_hpp */
