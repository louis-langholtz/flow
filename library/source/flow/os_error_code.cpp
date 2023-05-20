#include <sstream>
#include <system_error>

#include "flow/os_error_code.hpp"
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

[[noreturn]]
auto throw_error(os_error_code ec, const std::string& msg) -> void
{
    throw std::system_error{int(ec), std::system_category(), msg};
}

}
