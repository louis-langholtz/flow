#include <unistd.h> // for pid_t

#include "flow/reference_process_id.hpp"

namespace flow {

// Ensure our enum's underlying type matches for the pid_t's actual type!
static_assert(std::is_same_v<pid_t, std::underlying_type_t<reference_process_id>>);

auto operator<<(std::ostream& os, reference_process_id value) -> std::ostream&
{
    os << "pid:" << int(value);
    return os;
}

}
