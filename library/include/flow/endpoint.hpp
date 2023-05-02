#ifndef port_hpp
#define port_hpp

#include "flow/file_endpoint.hpp"
#include "flow/prototype_port.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

/// @brief Port.
/// @details Ports are connection endpoints to the variety of entities
/// available that provide endpoints for connections.
using endpoint = variant<prototype_port, file_endpoint>;

}

#endif /* port_hpp */
