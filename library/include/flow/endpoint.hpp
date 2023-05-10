#ifndef endpoint_hpp
#define endpoint_hpp

#include "flow/file_endpoint.hpp"
#include "flow/system_endpoint.hpp"
#include "flow/unset_endpoint.hpp"
#include "flow/user_endpoint.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

/// @brief Endpoint.
/// @details These are connection end points to the variety of entities
/// available that provide end-points for connections.
using endpoint = variant<
    unset_endpoint,
    user_endpoint,
    system_endpoint,
    file_endpoint
>;

}

#endif /* endpoint_hpp */
