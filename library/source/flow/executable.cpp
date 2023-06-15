#include "flow/executable.hpp"

namespace flow {

auto operator<<(std::ostream& os, const executable& value)
    -> std::ostream&
{
    os << "executable{";
    os << ".file=" << value.file;
    os << ",.arguments={";
    auto prefix = "";
    for (auto&& arg: value.arguments) {
        os << prefix << arg;
        prefix = ",";
    }
    os << "}";
    os << ",.working_directory=" << value.working_directory;
    os << "}";
    return os;
}

}
