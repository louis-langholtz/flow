#include "flow/descriptor_info.hpp"

namespace flow {

auto operator<<(std::ostream& os, const descriptor_info& value)
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
