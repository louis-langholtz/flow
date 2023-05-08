#include "flow/file_endpoint.hpp"

namespace flow {

const file_endpoint file_endpoint::dev_null = file_endpoint{"/dev/null"};

auto operator<<(std::ostream& os, const file_endpoint& value) -> std::ostream&
{
    os << "file_endpoint{";
    os << value.path;
    os << "}";
    return os;
}

}
