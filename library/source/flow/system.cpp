#include "flow/system.hpp"

namespace flow {

auto operator<<(std::ostream& os, const descriptor_map& value)
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

auto operator<<(std::ostream& os, const system& value)
    -> std::ostream&
{
    os << "system{";
    os << ".descriptors=" << value.descriptors << ",";
    os << ".environment=" << value.environment << ",";
    os << ".info=";
    if (const auto p = std::get_if<system::executable>(&(value.info))) {
        os << "executable_info{";
        os << ".path=" << p->executable_file;
        os << ",.working_directory=" << p->working_directory;
        os << "}";
    }
    else if (const auto p = std::get_if<system::custom>(&(value.info))) {
        os << "custom_info{";
        os << ".subsystems={";
        os << "},";
        os << ".connections={";
        os << "}";
        os << "}";
    }
    os << "}";
    return os;
}

}
