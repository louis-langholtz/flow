#include "flow/reference_descriptor.hpp"

namespace flow {

auto operator<<(std::ostream& os, reference_descriptor value) -> std::ostream&
{
    const auto number = static_cast<std::underlying_type_t<reference_descriptor>>(value);
    os << number;
    return os;
}

}
