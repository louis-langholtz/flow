#include "flow/process_id.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, process_id value)
{
    os << "pid:" << int(value);
    return os;
}

}
