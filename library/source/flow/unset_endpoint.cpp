#include "flow/unset_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const unset_endpoint&) -> std::ostream&
{
    os << "{}";
    return os;
}

}
