#include "flow/prototype_name.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const prototype_name& name)
{
    os << name.value;
    return os;
}

prototype_name operator+(const prototype_name& lhs, const prototype_name& rhs)
{
    auto result = std::string{};
    result += lhs.value;
    result += prototype_name::separator;
    result += rhs.value;
    return prototype_name{result};
}

}
