#include "process_name.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const process_name& name)
{
    os << name.value;
    return os;
}

}
