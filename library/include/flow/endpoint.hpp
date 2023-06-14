#ifndef endpoint_hpp
#define endpoint_hpp

#include <istream>

#include "flow/file_endpoint.hpp"
#include "flow/node_endpoint.hpp"
#include "flow/unset_endpoint.hpp"
#include "flow/user_endpoint.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

/// @brief Endpoint.
/// @details These are link end points to the variety of entities
/// available that provide end-points for links.
/// @note Every supported type minimally supports <code>operator==</code> so
///   that this variant too supports <code>operator==</code>.
using endpoint = variant<
    unset_endpoint,
    user_endpoint,
    node_endpoint,
    file_endpoint
>;

auto operator>>(std::istream& is, endpoint& value) -> std::istream&;

}

#endif /* endpoint_hpp */
