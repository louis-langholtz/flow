#include <type_traits>

#include <unistd.h> // for pid_t

#include "flow/process_id.hpp"

namespace flow {

// Ensure our enum's underlying type matches for the pid_t's actual type!
static_assert(std::is_same_v<pid_t, std::underlying_type_t<process_id>>);

std::ostream& operator<<(std::ostream& os, process_id value)
{
    os << "pid:" << int(value);
    return os;
}

}
