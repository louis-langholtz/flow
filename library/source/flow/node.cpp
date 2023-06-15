#include <sstream> // for std::ostringstream

#include "flow/indenting_ostreambuf.hpp"
#include "flow/node.hpp"
#include "flow/utility.hpp"

namespace flow {

auto operator<<(std::ostream& os, const node& value)
    -> std::ostream&
{
    os << "node{";
    auto top_prefix = "";
    if (!empty(value.interface)) {
        os << top_prefix << ".ports=" << value.interface;
        top_prefix = ",";
    }
    os << top_prefix << ".info=";
    if (const auto p = std::get_if<executable>(&(value.implementation))) {
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
    else if (const auto p = std::get_if<system>(&(value.implementation))) {
        os << "system{";
        auto sub_prefix = "";
        if (!empty(p->environment)) {
            os << sub_prefix << ".environment=" << p->environment;
            sub_prefix = ",";
        }
        if (!empty(p->nodes)) {
            os << ".nodes={";
            auto prefix = "";
            for (auto&& entry: p->nodes) {
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
        if (!empty(p->links)) {
            os << sub_prefix << ".links={";
            os << "}";
        }
        os << "}";
    }
    os << "}";
    return os;
}

auto pretty_print(std::ostream& os, const node& value) -> void
{
    os << "{\n";
    auto top_prefix = "";
    if (!empty(value.interface)) {
        os << top_prefix;
        os << "  .ports={";
        os << value.interface;
        os << "}";
        top_prefix = ",\n";
    }
    if (const auto p = std::get_if<system>(&value.implementation)) {
        os << top_prefix;
        os << "  .implementation=system{";
        auto info_prefix = "";
        if (!empty(p->environment)) {
            os << info_prefix;
            os << "\n";
            os << "  .environment={\n";
            {
                const auto opts = detail::indenting_ostreambuf_options{
                    4, true
                };
                const detail::indenting_ostreambuf child_indent{os, opts};
                pretty_print(os, p->environment);
            }
            os << "  }";
        }
        if (!empty(p->nodes)) {
            os << info_prefix;
            os << "\n";
            os << "    .nodes={\n";
            for (auto&& entry: p->nodes) {
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
        if (!empty(p->links)) {
            os << info_prefix;
            os << "\n";
            os << "    .links={\n";
            for (auto&& link: p->links) {
                os << "      " << link << ",\n";
            }
            os << "    }";
            info_prefix = ",";
        }
        if (*info_prefix) {
            os << "\n  ";
        }
        os << "}\n";
    }
    else if (const auto p = std::get_if<executable>(&value.implementation)) {
        os << top_prefix;
        os << "  .implementation=executable{";
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

auto get_matching_set(const node& sys, io_type io)
    -> std::set<port_id>
{
    return get_matching_set(sys.interface, io);
}

auto link_with_user(const node_name& name, const port_map& ports)
    -> std::vector<link>
{
    auto result = std::vector<link>{};
    for (auto&& entry: ports) {
        const auto user_ep_name = name.get() + ":" + to_string(entry.first);
        switch (entry.second.direction) {
        case io_type::in:
            result.emplace_back(unidirectional_link{
                user_endpoint{user_ep_name},
                node_endpoint{name, entry.first},
            });
            break;
        case io_type::out:
            result.emplace_back(unidirectional_link{
                node_endpoint{name, entry.first},
                user_endpoint{user_ep_name},
            });
            break;
        case io_type::bidir:
            result.emplace_back(bidirectional_link{
                node_endpoint{name, entry.first},
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
