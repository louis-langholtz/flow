#include "flow/process_port.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const process_port& value)
{
    os << value.address;
    os << ':';
    os << value.descriptor;
    return os;
}

}
