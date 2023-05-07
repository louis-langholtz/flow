#include "flow/system_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const system_endpoint& value) -> std::ostream&
{
    os << "system_endpoint{";
    os << value.address;
    os << ':';
    auto prefix = "";
    for (auto&& descriptor: value.descriptors) {
        os << prefix << descriptor;
        prefix = ",";
    }
    os << "}";
    return os;
}

}
