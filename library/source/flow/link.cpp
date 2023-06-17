#include "flow/link.hpp"

namespace flow {

auto operator<<(std::ostream& os, const link& value) -> std::ostream&
{
    os << "link{" << value.a << "," << value.b << "}";
    return os;
}

}
