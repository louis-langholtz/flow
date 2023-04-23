#include "flow/prototype_port.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const prototype_port& value)
{
    os << value.address;
    os << ':';
    os << value.descriptor;
    return os;
}

}
