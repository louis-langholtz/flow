#include <fcntl.h> // for open flags

#include "flow/io_type.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, io_type value)
{
    os << to_cstring(value);
    return os;
}

}
