#include "flow/connection.hpp"

namespace flow {

auto operator<<(std::ostream& os, const unidirectional_connection& value)
    -> std::ostream&
{
    os << "unidirectional_connection{";
    os << ".src=" << value.src;
    os << ",.dst=" << value.dst;
    os << "}";
    return os;
}

auto operator<<(std::ostream& os, const bidirectional_connection& value)
    -> std::ostream&
{
    os << "bidirectional_connection{";
    auto prepend = "";
    for (auto&& end: value.ends) {
        os << prepend << end;
        prepend = ",";
    }
    os << "}";
    return os;
}

}
