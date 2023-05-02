#include "flow/connection.hpp"

namespace flow {

auto operator<<(std::ostream& os, const connection& value)
    -> std::ostream&
{
    os << "connection{";
    auto prepend = "";
    for (auto&& end: value.ends) {
        os << prepend << end;
        prepend = ",";
    }
    os << "}";
    return os;
}

}
