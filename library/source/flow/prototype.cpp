#include "flow/prototype.hpp"

namespace flow {

auto operator<<(std::ostream& os, const descriptor_container& value)
    -> std::ostream&
{
    os << "{";
    for (auto&& entry: value) {
        if (&entry != &(*value.begin())) {
            os << ",";
        }
        os << "{";
        // TODO
        os << "}";
    }
    os << "}";
    return os;
}

auto operator<<(std::ostream& os, const executable_prototype& value)
    -> std::ostream&
{
    os << "executable_prototype{";
    os << ".descriptors=" << value.descriptors;
    os << ".path=" << value.executable_file;
    os << ",.working_directory=" << value.working_directory;
    // TODO: other members
    os << "}";
    return os;
}

auto operator<<(std::ostream& os, const system_prototype& value)
    -> std::ostream&
{
    os << "system_prototype{";
    os << ".descriptors=" << value.descriptors;
    os << "}";
    return os;
}

}
