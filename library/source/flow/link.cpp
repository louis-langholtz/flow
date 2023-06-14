#include "flow/link.hpp"

namespace flow {

auto operator<<(std::ostream& os, const unidirectional_link& value)
    -> std::ostream&
{
    os << "unidirectional_link{";
    os << ".src=" << value.src;
    os << ",.dst=" << value.dst;
    os << "}";
    return os;
}

auto operator<<(std::ostream& os, const bidirectional_link& value)
    -> std::ostream&
{
    os << "bidirectional_link{";
    auto prepend = "";
    for (auto&& end: value.ends) {
        os << prepend << end;
        prepend = ",";
    }
    os << "}";
    return os;
}

}
