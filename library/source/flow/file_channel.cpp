#include "flow/file_channel.hpp"

namespace flow {

auto operator<<(std::ostream& os, const file_channel&) -> std::ostream&
{
    os << "file_channel{}";
    return os;
}

}
