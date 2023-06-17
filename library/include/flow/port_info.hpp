#ifndef port_info_hpp
#define port_info_hpp

#include <concepts> // for std::regular.
#include <ostream>
#include <string>

#include "flow/io_type.hpp"

namespace flow {

struct port_info {
    std::string comment;
    io_type direction;
};

inline auto operator==(const port_info& lhs,
                       const port_info& rhs) noexcept
{
    return (lhs.comment == rhs.comment) && (lhs.direction == rhs.direction);
}

static_assert(std::regular<port_info>);

auto operator<<(std::ostream& os, const port_info& value)
    -> std::ostream&;

}

#endif /* port_info_hpp */
