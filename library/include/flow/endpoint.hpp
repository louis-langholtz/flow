#ifndef endpoint_hpp
#define endpoint_hpp

#include <istream>

#include "flow/file_endpoint.hpp"
#include "flow/system_endpoint.hpp"
#include "flow/unset_endpoint.hpp"
#include "flow/user_endpoint.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

/// @brief Endpoint.
/// @details These are connection end points to the variety of entities
/// available that provide end-points for links.
/// @note Every supported type minimally supports <code>operator==</code> so
///   that this variant too supports <code>operator==</code>.
using endpoint = variant<
    unset_endpoint,
    user_endpoint,
    system_endpoint,
    file_endpoint
>;

auto operator>>(std::istream& is, endpoint& value) -> std::istream&;

}

#endif /* endpoint_hpp */
