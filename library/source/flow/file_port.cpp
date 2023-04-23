#include "file_port.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const file_port& value)
{
    os << value.path;
    return os;
}

}
