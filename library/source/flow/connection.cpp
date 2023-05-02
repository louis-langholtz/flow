#include "flow/connection.hpp"

namespace flow {

auto operator<<(std::ostream& os, const connection& value)
    -> std::ostream&
{
    os << "connection{";
    auto prepend = "";
    for (auto&& port: value.ends) {
        os << prepend << port;
        prepend = ",";
    }
    os << "}";
    return os;
}

}
