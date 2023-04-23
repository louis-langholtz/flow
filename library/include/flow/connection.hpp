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

/// @brief Pipe connection.
/// @details A unidirectional connection between prototypes.
/// @note Results in a <code>pipe_channel</code>.
struct pipe_connection {
    prototype_port in;
    prototype_port out;
};

using connection = std::variant<pipe_connection, file_connection>;

std::ostream& operator<<(std::ostream& os, const file_connection& value);
std::ostream& operator<<(std::ostream& os, const pipe_connection& value);

}

#endif /* connection_hpp */
