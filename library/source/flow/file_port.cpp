#include "flow/file_port.hpp"

namespace flow {

auto operator<<(std::ostream& os, const file_port& value) -> std::ostream&
{
    os << value.path;
    return os;
}

}
