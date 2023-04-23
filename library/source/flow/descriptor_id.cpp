#include "flow/descriptor_id.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, descriptor_id value)
{
    os << "fd:" << int(value);
    return os;
}

}
