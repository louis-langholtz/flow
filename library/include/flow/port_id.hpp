#ifndef port_id_hpp
#define port_id_hpp

#include <string>
#include <type_traits> // for std::is_default_constructible_v

#include "flow/reference_descriptor.hpp"
#include "flow/variant.hpp"

namespace flow {

using port_id = variant<
    reference_descriptor
>;

template <class T>
concept less_comparable = requires(const T& v) {
    std::less<T>{}(v, v);
};

static_assert(std::is_default_constructible_v<port_id>);
static_assert(less_comparable<port_id>); // assures works as map key

auto to_string(const port_id& value) -> std::string;

}

#endif /* port_id_hpp */
