#ifndef descriptor_info_hpp
#define descriptor_info_hpp

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

auto operator<<(std::ostream& os, const port_info& value)
    -> std::ostream&;

}

#endif /* descriptor_info_hpp */
