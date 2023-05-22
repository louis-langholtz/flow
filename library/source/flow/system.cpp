#include <sstream> // for std::ostringstream

#include "flow/indenting_ostreambuf.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace flow {

auto operator<<(std::ostream& os, const system& value)
    -> std::ostream&
{
    os << "system{";
    auto top_prefix = "";
    if (!empty(value.descriptors)) {
        os << top_prefix << ".descriptors=" << value.descriptors;
        top_prefix = ",";
    }
    if (!empty(value.environment)) {
        os << top_prefix << ".environment=" << value.environment;
        top_prefix = ",";
    }
    os << top_prefix << ".info=";
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
        auto sub_prefix = "";
        if (!empty(p->subsystems)) {
            os << ".subsystems={";
            auto prefix = "";
            for (auto&& entry: p->subsystems) {
                os << prefix << "{";
                os << entry.first;
                os << "=";
                os << entry.second;
                os << "}";
                prefix = ",";
            }
            os << "}";
            sub_prefix = ",";
        }
        if (!empty(p->connections)) {
            os << sub_prefix << ".connections={";
            os << "}";
        }
        os << "}";
    }
    os << "}";
    return os;
}

auto pretty_print(std::ostream& os, const system& value) -> void
{
    os << "{\n";
    auto top_prefix = "";
    if (!empty(value.descriptors)) {
        os << top_prefix;
        os << "  .descriptors={";
        os << value.descriptors;
        os << "}";
        top_prefix = ",\n";
    }
    if (!empty(value.environment)) {
        os << top_prefix;
        os << "  .environment={\n";
        {
            const auto opts = detail::indenting_ostreambuf_options{
                4, true
            };
            const detail::indenting_ostreambuf child_indent{os, opts};
            pretty_print(os, value.environment);
        }
        os << "  }";
        top_prefix = ",\n";
    }
    if (const auto p = std::get_if<system::custom>(&value.info)) {
        os << top_prefix;
        os << "  .info=custom{";
        auto info_prefix = "";
        if (!empty(p->subsystems)) {
            os << info_prefix;
            os << "\n";
            os << "    .subsystems={\n";
            for (auto&& entry: p->subsystems) {
                os << "      {\n";
                os << "        .first=" << entry.first << ",\n";
                os << "        .second=";
                {
                    const auto opts = detail::indenting_ostreambuf_options{
                        8, false
                    };
                    const detail::indenting_ostreambuf child_indent{os, opts};
                    pretty_print(os, entry.second);
                }
                os << "      },\n";
            }
            os << "    }";
            info_prefix = ",";
        }
        if (!empty(p->connections)) {
            os << info_prefix;
            os << "\n";
            os << "    .connections={\n";
            for (auto&& connection: p->connections) {
                os << "      " << connection << ",\n";
            }
            os << "    }";
            info_prefix = ",";
        }
        if (*info_prefix) {
            os << "\n  ";
        }
        os << "}\n";
    }
    else if (const auto p = std::get_if<system::executable>(&value.info)) {
        os << top_prefix;
        os << "  .info=executable{";
        auto exe_prefix = "\n";
        if (!p->file.empty()) {
            os << exe_prefix;
            os << "    .file=";
            os << p->file;
            exe_prefix = ",\n";
        }
        if (!empty(p->arguments)) {
            os << exe_prefix;
            os << "    .args={";
            auto arg_prefix = "";
            for (auto&& arg: p->arguments) {
                os << arg_prefix << arg;
                arg_prefix = ",";
            }
            os << "}\n";
        }
        else {
            os << "\n";
        }
        os << "  }\n";
    }
    os << "}\n";
}

auto get_matching_set(const system& sys, io_type io)
    -> std::set<reference_descriptor>
{
    return get_matching_set(sys.descriptors, io);
}

auto connect_with_user(const system_name& name,
                       const descriptor_map& descriptors)
    -> std::vector<connection>
{
    auto result = std::vector<connection>{};
    for (auto&& entry: descriptors) {
        const auto user_ep_name = name.get() + ":" +
            std::to_string(int(entry.first));
        switch (entry.second.direction) {
        case io_type::in:
            result.emplace_back(unidirectional_connection{
                user_endpoint{user_ep_name},
                system_endpoint{name, entry.first},
            });
            break;
        case io_type::out:
            result.emplace_back(unidirectional_connection{
                system_endpoint{name, entry.first},
                user_endpoint{user_ep_name},
            });
            break;
        case io_type::bidir:
            result.emplace_back(bidirectional_connection{
                system_endpoint{name, entry.first},
                user_endpoint{user_ep_name},
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
