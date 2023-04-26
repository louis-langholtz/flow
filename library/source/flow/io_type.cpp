#include <fcntl.h> // for open flags

#include "flow/io_type.hpp"

namespace flow {

int to_open_flags(io_type direction)
{
    switch (direction) {
    case io_type::in: return O_RDONLY;
    case io_type::out: return O_WRONLY;
    }
    throw std::logic_error{"unknown direction"};
}

std::ostream& operator<<(std::ostream& os, io_type value)
{
    os << to_cstring(value);
    return os;
}

}
