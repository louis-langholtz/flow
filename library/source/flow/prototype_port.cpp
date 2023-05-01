#include "flow/prototype_port.hpp"

namespace flow {

auto operator<<(std::ostream& os, const prototype_port& value) -> std::ostream&
{
    os << "prototype_port{";
    os << value.address;
    os << ':';
    os << value.descriptor;
    os << "}";
    return os;
}

}
