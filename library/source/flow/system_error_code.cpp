#include <sstream>
#include <system_error>

#include "flow/system_error_code.hpp"
#include "flow/utility.hpp"

namespace flow {

auto operator<<(std::ostream& os, system_error_code err)
    -> std::ostream&
{
    return write(os, std::error_code{int(err), std::system_category()});
}

auto to_string(system_error_code err) -> std::string
{
    std::ostringstream os;
    os << err;
    return os.str();
}

}
