#include "flow/prototype_name.hpp"

namespace flow {

auto operator<<(std::ostream& os, const prototype_name& name) -> std::ostream&
{
    os << name.value;
    return os;
}

auto operator+(const prototype_name& lhs, const prototype_name& rhs)
    -> prototype_name
{
    auto result = std::string{};
    result += lhs.value;
    result += prototype_name::separator;
    result += rhs.value;
    return prototype_name{result};
}

}
