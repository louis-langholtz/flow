#include "flow/file_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const file_endpoint& value) -> std::ostream&
{
    os << value.path;
    return os;
}

}
