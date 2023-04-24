#include "flow/prototype_port.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const prototype_port& value)
{
    os << "prototype_port{";
    os << value.address;
    os << ':';
    os << value.descriptor;
    os << "}";
    return os;
}

}
