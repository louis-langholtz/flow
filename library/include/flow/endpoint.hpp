#ifndef endpoint_hpp
#define endpoint_hpp

#include "flow/file_endpoint.hpp"
#include "flow/prototype_endpoint.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

/// @brief Endpoint.
/// @details These are connection end points to the variety of entities
/// available that provide end points for connections.
using endpoint = variant<prototype_endpoint, file_endpoint>;

}

#endif /* endpoint_hpp */
