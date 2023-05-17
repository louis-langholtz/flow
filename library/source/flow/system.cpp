#include <sstream> // for std::ostringstream

#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace flow {

auto operator<<(std::ostream& os, const system& value)
    -> std::ostream&
{
    os << "system{";
    os << ".descriptors=" << value.descriptors;
    os << ",.environment=" << value.environment;
    os << ",.info=";
    if (const auto p = std::get_if<system::executable>(&(value.info))) {
        os << "executable_info{";
        os << ".file=" << p->file;
        os << ",.arguments={";
        auto prefix = "";
        for (auto&& arg: p->arguments) {
            os << prefix << arg;
            prefix = ",";
        }
        os << "}";
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

auto connect_with_user(const system_name& name,
                       const descriptor_map& descriptors)
    -> std::vector<connection>
{
    auto result = std::vector<connection>{};
    for (auto&& entry: descriptors) {
        switch (entry.second.direction) {
        case io_type::in:
            result.emplace_back(unidirectional_connection{
                user_endpoint{},
                system_endpoint{name, entry.first},
            });
            break;
        case io_type::out:
            result.emplace_back(unidirectional_connection{
                system_endpoint{name, entry.first},
                user_endpoint{},
            });
            break;
        case io_type::bidir:
            result.emplace_back(bidirectional_connection{
                system_endpoint{name, entry.first},
                user_endpoint{},
            });
            break;
        case io_type::none:
        default: {
            std::ostringstream os;
            os << "unexpected descriptor map entry direction of ";
            os << entry.second.direction;
            os << " (";
            os << std::underlying_type_t<io_type>(entry.second.direction);
            os << ")";
            throw std::invalid_argument{os.str()};
        }
        }
    }
    return result;
}

}
