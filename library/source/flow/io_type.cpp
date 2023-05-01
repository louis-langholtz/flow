#include <fcntl.h> // for open flags

#include "flow/io_type.hpp"

namespace flow {

auto operator<<(std::ostream& os, io_type value) -> std::ostream&
{
    os << to_cstring(value);
    return os;
}

}
