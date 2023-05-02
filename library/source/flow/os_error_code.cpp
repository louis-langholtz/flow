#include <sstream>
#include <system_error>

#include "os_error_code.hpp"

#include "flow/utility.hpp"

namespace flow {

auto operator<<(std::ostream& os, os_error_code err)
    -> std::ostream&
{
    return write(os, std::error_code{int(err), std::system_category()});
}

auto to_string(os_error_code err) -> std::string
{
    std::ostringstream os;
    os << err;
    return os.str();
}

}
