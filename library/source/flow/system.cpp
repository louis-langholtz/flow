#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace flow {

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

auto get_matching_set(const system& sys, io_type io)
    -> std::set<descriptor_id>
{
    return get_matching_set(sys.descriptors, io);
}

}
