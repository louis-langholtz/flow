#include "flow/signal_channel.hpp"

namespace flow {

auto operator<<(std::ostream& os, const signal_channel& value)
    -> std::ostream&
{
    os << "{";
    auto prefix = "";
    for (auto&& signal: value.signals) {
        os << prefix << signal;
        prefix = ",";
    }
    os << "}->";
    os << value.address;
    return os;
}

}
