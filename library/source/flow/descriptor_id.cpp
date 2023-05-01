#include "flow/descriptor_id.hpp"

namespace flow {

auto operator<<(std::ostream& os, descriptor_id value) -> std::ostream&
{
    const auto number = static_cast<std::underlying_type_t<descriptor_id>>(value);
    os << number;
    return os;
}

}
