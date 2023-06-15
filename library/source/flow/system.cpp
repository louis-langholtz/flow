#include "flow/node.hpp"
#include "flow/system.hpp"

namespace flow {

auto operator==(const system& lhs, const system& rhs) noexcept -> bool
{
    return (lhs.nodes == rhs.nodes)
        && (lhs.environment == rhs.environment)
        && (lhs.links == rhs.links);
}

auto operator<<(std::ostream& os, const system& value)
    -> std::ostream&
{
    os << "system{";
    auto sub_prefix = "";
    if (!empty(value.environment)) {
        os << sub_prefix << ".environment=" << value.environment;
        sub_prefix = ",";
    }
    if (!empty(value.nodes)) {
        os << ".nodes={";
        auto prefix = "";
        for (auto&& entry: value.nodes) {
            os << prefix << "{";
            os << entry.first;
            os << "=";
            os << entry.second;
            os << "}";
            prefix = ",";
        }
        os << "}";
        sub_prefix = ",";
    }
    if (!empty(value.links)) {
        os << sub_prefix << ".links={";
        os << "}";
    }
    os << "}";
    return os;
}

}
