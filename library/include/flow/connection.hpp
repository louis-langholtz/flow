#ifndef connection_hpp
#define connection_hpp

#include <array>
#include <ostream>

#include "flow/file_port.hpp"
#include "flow/prototype_port.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

using port = variant<prototype_port, file_port>;

struct connection {
    std::array<port, 2u> ports;
};

constexpr auto operator==(const connection& lhs,
                          const connection& rhs) noexcept -> bool
{
    return lhs.ports == rhs.ports;
}

auto operator<<(std::ostream& os, const connection& value)
    -> std::ostream&;
}

#endif /* connection_hpp */
