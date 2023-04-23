#include "flow/prototype_name.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const prototype_name& name)
{
    os << name.value;
    return os;
}

}
