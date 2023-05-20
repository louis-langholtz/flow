#include "flow/reference_descriptor.hpp"

namespace flow {

auto operator<<(std::ostream& os, reference_descriptor value) -> std::ostream&
{
    using underlying_type = std::underlying_type_t<reference_descriptor>;
    const auto number = static_cast<underlying_type>(value);
    os << number;
    return os;
}

}
