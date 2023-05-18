#include <iomanip> // for std::quoted

#include "flow/system_name.hpp"

namespace flow {

auto operator<<(std::ostream& os, const system_name& name) -> std::ostream&
{
    os << std::quoted(name.get());
    return os;
}

auto operator<<(std::ostream& os,
                const std::vector<system_name>& names) -> std::ostream&
{
    auto prefix = "";
    for (auto&& component: names) {
        os << prefix << component;
        prefix = system_name_checker::separator;
    }
    return os;
}

}
