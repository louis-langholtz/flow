#include <sstream>

#include "flow/port_id.hpp"

namespace flow {

auto to_string(const port_id& value) -> std::string
{
    std::ostringstream os;
    os << value;
    return os.str();
}

}
