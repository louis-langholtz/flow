#include "flow/system_name.hpp"

namespace flow {

auto operator<<(std::ostream& os, const system_name& name) -> std::ostream&
{
    os << name.value;
    return os;
}

auto operator+(const system_name& lhs, const system_name& rhs)
    -> system_name
{
    auto result = std::string{};
    result += lhs.value;
    result += system_name::separator;
    result += rhs.value;
    return system_name{result};
}

}
