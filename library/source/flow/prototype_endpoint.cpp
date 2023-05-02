#include "flow/prototype_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const prototype_endpoint& value) -> std::ostream&
{
    os << "prototype_port{";
    os << value.address;
    os << ':';
    os << value.descriptor;
    os << "}";
    return os;
}

}
