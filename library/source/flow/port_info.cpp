#include "flow/port_info.hpp"

namespace flow {

auto operator<<(std::ostream& os, const port_info& value)
    -> std::ostream&
{
    os << value.direction;
    if (!empty(value.comment)) {
        os << ":";
        os << value.comment;
    }
    return os;
}

}
