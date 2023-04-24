#ifndef connection_hpp
#define connection_hpp

#include <ostream>
#include <variant>

#include "flow/file_port.hpp"
#include "flow/io_type.hpp"
#include "flow/prototype_port.hpp"

namespace flow {

struct file_connection {
    file_port file;
    io_type direction;
    prototype_port process;
};

constexpr auto operator==(const file_connection& lhs,
                          const file_connection& rhs) -> bool
{
    return (lhs.file == rhs.file)
        && (lhs.direction == rhs.direction)
        && (lhs.process == rhs.process);
}

auto operator<<(std::ostream& os, const file_connection& value) -> std::ostream&;

/// @brief Pipe connection.
/// @details A unidirectional connection between prototypes.
/// @note Results in a <code>pipe_channel</code>.
struct pipe_connection {
    prototype_port in;
    prototype_port out;
};

constexpr auto operator==(const pipe_connection& lhs,
                          const pipe_connection& rhs) -> bool
{
    return (lhs.in == rhs.in) && (lhs.out == rhs.out);
}

auto operator<<(std::ostream& os, const pipe_connection& value) -> std::ostream&;

using connection = std::variant<pipe_connection, file_connection>;

}

#endif /* connection_hpp */
