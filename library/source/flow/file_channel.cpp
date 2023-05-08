#include "flow/file_channel.hpp"

namespace flow {

auto operator<<(std::ostream& os, const file_channel& value) -> std::ostream&
{
    os << "file_channel{";
    os << value.path;
    os << ",";
    os << value.io;
    os << "}";
    return os;
}

}
