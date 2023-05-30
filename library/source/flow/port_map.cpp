#include "flow/port_map.hpp"

namespace flow {

auto operator<<(std::ostream& os, const port_map& value)
    -> std::ostream&
{
    os << "{";
    auto prefix = "";
    for (auto&& entry: value) {
        os << prefix;
        os << "{";
        os << entry.first;
        os << ",";
        os << entry.second;
        os << "}";
        prefix = ",";
    }
    os << "}";
    return os;
}

}
