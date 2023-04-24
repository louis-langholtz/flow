#include "flow/descriptor_id.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, descriptor_id value)
{
    const auto number = static_cast<std::underlying_type_t<descriptor_id>>(value);
    os << number;
    return os;
}

}
