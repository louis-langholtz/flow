#include "flow/prototype.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const descriptor_container& value)
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

std::ostream& operator<<(std::ostream& os, const executable_prototype& value)
{
    os << "executable_prototype{";
    os << ".descriptors=" << value.descriptors;
    os << ".path=" << value.executable_file;
    os << ",.working_directory=" << value.working_directory;
    // TODO: other members
    os << "}";
    return os;
}

std::ostream& operator<<(std::ostream& os, const system_prototype& value)
{
    os << "system_prototype{";
    os << ".descriptors=" << value.descriptors;
    os << "}";
    return os;
}

std::ostream& operator<<(std::ostream& os, const prototype& value)
{
    if (const auto c = std::get_if<executable_prototype>(&value)) {
        os << *c;
    }
    else if (const auto c = std::get_if<system_prototype>(&value)) {
        os << *c;
    }
    return os;
}

}
