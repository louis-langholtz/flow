#include "flow/system_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const system_endpoint& value) -> std::ostream&
{
    os << "system_endpoint{";
    os << value.address;
    os << ':';
    os << value.descriptor;
    os << "}";
    return os;
}

}
