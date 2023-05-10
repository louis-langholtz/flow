#ifndef descriptor_info_hpp
#define descriptor_info_hpp

#include <ostream>
#include <string>

#include "flow/io_type.hpp"

namespace flow {

struct descriptor_info {
    std::string comment;
    io_type direction;
};

constexpr auto operator==(const descriptor_info& lhs,
                          const descriptor_info& rhs) noexcept
{
    return (lhs.comment == rhs.comment) && (lhs.direction == rhs.direction);
}

auto operator<<(std::ostream& os, const descriptor_info& value)
    -> std::ostream&;

}

#endif /* descriptor_info_hpp */
