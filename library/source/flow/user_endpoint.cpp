#include "flow/user_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const user_endpoint& /*value*/)
    -> std::ostream&
{
    os << "user_endpoint{}";
    return os;
}

}
