#include "flow/file_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const file_endpoint& value) -> std::ostream&
{
    os << "file_endpoint{";
    os << value.path;
    os << "}";
    return os;
}

}
