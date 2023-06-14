#include <cstdlib> // for std::strtol
#include <functional> // for std::function, std::reference_wrapper
#include <iomanip> // for std::quoted
#include <iostream>
#include <iterator>
#include <iomanip> // for std::setw
#include <memory> // for std::unique_ptr
#include <ostream> // for std::flush
#include <span>
#include <stack>
#include <string> // for std::getline
#include <queue>

#include <histedit.h>

#include "flow/channel.hpp"
#include "flow/charset_checker.hpp"
#include "flow/connection.hpp"
#include "flow/reference_descriptor.hpp"
#include "flow/environment_map.hpp"
#include "flow/indenting_ostreambuf.hpp"
#include "flow/instantiate.hpp"
#include "flow/utility.hpp"

namespace {

using arguments = std::vector<std::string>;
using string_span = std::span<const std::string>;
using system_stack_type = std::stack<std::reference_wrapper<flow::node>>;

using cmd_handler = std::function<void(const string_span& args)>;
using cmd_table = std::map<std::string, cmd_handler>;

constexpr auto shell_name = "flow";

const auto des_prefix = std::string{"+"};
const auto name_prefix = std::string{"--name="};
const auto parent_prefix = std::string{"--parent="};
const auto file_prefix = std::string{"--file="};
const auto src_prefix = std::string{"--src="};
const auto dst_prefix = std::string{"--dst="};
const auto rebase_prefix = std::string{"--rebase="};
const auto help_argument = std::string{"--help"};
const auto usage_argument = std::string{"--usage"};
const auto background_argument = std::string{"&"};

constexpr auto emacs_editor_str = "emacs";
constexpr auto vi_editor_str = "vi";
constexpr auto assignment_token = '=';
constexpr auto custom_begin_token = "{";
constexpr auto custom_end_token = "}";
constexpr auto connection_separator = '-';

auto next_sequence() -> std::size_t
{
    static auto sequence = std::size_t{0};
    return ++sequence;
}

auto bg_job_name(const std::string_view& cmd) -> std::string
{
    auto result = std::string{cmd};
    result += '_';
    result += std::to_string(next_sequence());
    return result;
}

/// @brief Makes the stated return type from given argument count and vector.
/// @param[in] ac Argument count.
/// @param[in] av Argument vector.
/// @pre @c av is non-null if @c ac is 1 or more.
auto make_arguments(int ac, const char*av[]) -> arguments
{
    assert(ac <= 0 || av);
    auto args = arguments{};
    for (auto i = 0; i < ac; ++i) {
        args.emplace_back(av[i]);
    }
    return args;
}

template <class T, class U>
auto operator==(const std::span<T>& lhs, const std::span<U>& rhs) ->
decltype(T{} == U{})
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (auto i = std::size_t{}; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

struct EditLineDeleter
{
    void operator()(EditLine *p)
    {
        el_end(p);
    }
};

using edit_line_ptr = std::unique_ptr<EditLine, EditLineDeleter>;

struct HistoryDeleter
{
    void operator()(History *p)
    {
        history_end(p);
    }
};

using history_ptr = std::unique_ptr<History, HistoryDeleter>;

struct TokenizerDeleter
{
    void operator()(Tokenizer *p)
    {
        tok_end(p);
    }
};

using tokenizer_ptr = std::unique_ptr<Tokenizer, TokenizerDeleter>;

auto continuation = false;

char *prompt([[maybe_unused]] EditLine *el)
{
    static auto nl_prefix = std::string{"\1\033[7m\1"};
    static auto nl_suffix = std::string{"$\1\033[0m\1 "};
    static auto nl_buf = nl_prefix + shell_name + nl_suffix;
    static auto cl_buf = std::string{shell_name} + "> ";
    return continuation? cl_buf.data(): nl_buf.data();
}

auto find(const std::map<flow::system_name, flow::node>& map,
          const std::string_view& name) -> const flow::node*
{
    auto sname = flow::system_name{};
    try {
        sname = flow::system_name{name};
    }
    catch (const flow::charset_validator_error& ex) {
        return nullptr;
    }
    const auto entry = map.find(sname);
    if (entry == map.end()) {
        return nullptr;
    }
    return &entry->second;
}

auto find(const system_stack_type& stack, const arguments& args)
    -> const flow::node*
{
    const auto& custom = std::get<flow::custom>(stack.top().get().implementation);
    for (auto&& entry: custom.nodes) {
        const auto& sub = entry.second;
        if (const auto p = std::get_if<flow::executable>(&sub.implementation)) {
            if (p->arguments == args) {
                return &sub;
            }
        }
    }
    if (const auto found = find(custom.nodes, args[0])) {
        return found;
    }
    return nullptr;
}

auto update(const flow::node& system, const string_span& args) -> flow::node
{
    flow::node tsys = system;
    if (const auto p = std::get_if<flow::executable>(&tsys.implementation)) {
        if (size(args) > 1u) {
            const auto cmd = empty(p->arguments)
                ? p->file.native()
                : p->arguments[0];
            p->arguments.clear();
            std::copy(data(args), data(args) + size(args),
                      std::back_inserter(p->arguments));
            if (!empty(cmd)) {
                p->arguments[0] = cmd;
            }
        }
    }
    return tsys;
}

auto to_int(const std::string_view& view)
    -> std::optional<int>
{
    char* p_end{};
    const auto n = std::strtol(data(view), &p_end, 10);
    if ((p_end == data(view)) || (p_end != (data(view) + size(view)))) {
        return {};
    }
    return {n};
}

auto parse_assignment(const std::string_view& arg)
    -> std::optional<std::pair<std::string, std::string>>
{
    const auto found = arg.find(assignment_token);
    if (found == std::string_view::npos) {
        return {};
    }
    return std::pair<std::string, std::string>{
        arg.substr(0u, found), arg.substr(found + 1u)
    };
}

auto parse_port_map_entry(const std::string_view& arg)
    -> std::optional<flow::port_map_entry>
{
    const auto name_value = parse_assignment(arg);
    if (!name_value) {
        std::cerr << "aborting: no '";
        std::cerr << assignment_token;
        std::cerr << "' found in ";
        std::cerr << "descriptor map entry argument\n";
        return {};
    }
    const auto found_int = to_int(name_value->first);
    if (!found_int) {
        std::cerr << "aborting: invalid key ";
        std::cerr << std::quoted(name_value->first);
        std::cerr << "\n";
        return {};
    }
    auto dir_comp = std::string_view{};
    auto des_info = flow::port_info{};
    if (const auto found = name_value->second.find(':');
        found != std::string::npos) {
        des_info.comment = name_value->second.substr(found + 1u);
        dir_comp = name_value->second.substr(0, found);
    }
    else {
        dir_comp = name_value->second;
    }
    if (const auto found = flow::to_io_type(dir_comp)) {
        des_info.direction = *found;
    }
    else {
        std::cerr << "aborting: unrecognized direction in ";
        std::cerr << std::quoted(dir_comp);
        std::cerr << "\n";
        return {};
    }
    return {flow::port_map_entry{
        flow::reference_descriptor(*found_int), des_info
    }};
}

auto update(flow::port_map& map, const flow::port_map_entry& entry)
    -> void
{
    if (entry.second.direction == flow::io_type::none) {
        map.erase(entry.first);
        return;
    }
    if (const auto ret = map.emplace(entry); !ret.second) {
        ret.first->second = entry.second;
    }
}

struct system_basis
{
    /// @brief Names from which the map of systems is arrived at.
    std::deque<flow::system_name> names;

    /// @brief Remaining node names that have yet to be parsed through
    ///   possibly because no matches for them have been found yet.
    std::deque<flow::system_name> remaining;

    /// @brief Pointer to map of systems arrived at from @root.
    flow::node *psystem;

    auto operator==(const system_basis& other) const noexcept -> bool = default;
};

auto parse(system_basis from, std::size_t n_remain = 0u) -> system_basis
{
    while (size(from.remaining) > n_remain) {
        const auto p = std::get_if<flow::custom>(&from.psystem->implementation);
        if (!p) {
            break;
        }
        const auto found = p->nodes.find(from.remaining.front());
        if (found == p->nodes.end()) {
            break;
        }
        from.psystem = &found->second;
        from.names.emplace_back(from.remaining.front());
        from.remaining.pop_front();
    }
    return from;
}

auto parse(flow::endpoint& value, const std::string& string) -> bool
{
    std::stringstream ss;
    ss << string;
    ss >> value;
    return !ss.fail();
}

auto do_unset_system(flow::node& context, const string_span& args) -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " ";
        os << help_argument;
        os << "|";
        os << usage_argument;
        os << "|<existing-system-name>...\n";
    };
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "removes system definitions.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        auto sys_names = std::deque<flow::system_name>{};
        try {
            sys_names = flow::to_system_names(arg);
        }
        catch (const flow::charset_validator_error& ex) {
            std::cerr << "invalid name ";
            std::cerr << arg;
            std::cerr << ": ";
            std::cerr << ex.what();
            std::cerr << "\n";
            continue;
        }
        const auto arg_basis = parse({{}, sys_names, &context}, 1u);
        const auto p =
            std::get_if<flow::custom>(&arg_basis.psystem->implementation);
        if (!p) {
            std::cerr << "parent not custom?!\n";
            continue;
        }
        if (size(arg_basis.remaining) > 1u) {
            std::cerr << "no such system as " << arg << "\n";
            continue;
        }
        if (p->nodes.erase(arg_basis.remaining.front()) == 0) {
            std::cerr << arg << " not found\n";
        }
    }
}

auto get_instantiate_options(const flow::node& context)
{
    return flow::instantiate_options{
        .ports = context.interface,
        .environment = std::get<flow::custom>(context.implementation).environment
    };
}

auto instantiate(const std::string_view& cmd,
                 const flow::node& tsys,
                 const flow::instantiate_options& opts)
    -> std::optional<flow::instance>
{
    try {
        return {instantiate(tsys, std::cerr, opts)};
    }
    catch (const std::invalid_argument& ex) {
        std::cerr << "cannot instantiate ";
        std::cerr << std::quoted(cmd);
        std::cerr << ": ";
        std::cerr << ex.what();
        std::cerr << "\n";
    }
    return {};
}

auto print(const std::span<const flow::wait_result>& results,
           bool verbose = false) -> void
{
    for (auto&& result: results) {
        if (!std::holds_alternative<flow::info_wait_result>(result)) {
            std::cerr << result << "\n";
            continue;
        }
        const auto& info = std::get<flow::info_wait_result>(result);
        if (!std::holds_alternative<flow::wait_exit_status>(info.status)) {
            std::cerr << result << "\n";
            continue;
        }
        const auto& stat = std::get<flow::wait_exit_status>(info.status);
        if (stat.value != 0) {
            std::cerr << result << "\n";
            continue;
        }
        if (verbose) {
            std::cout << result << "\n";
        }
    }
}

auto foreground(const std::string_view& cmd,
                const flow::node& tsys,
                const flow::instantiate_options& opts)
{
    if (auto obj = instantiate(cmd, tsys, opts)) {
        print(wait(*obj));
        write_diags(*obj, std::cerr, cmd);
    }
}

auto do_foreground(const flow::node& context, const string_span& args)
    -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " ";
        os << help_argument;
        os << "|";
        os << usage_argument;
        os << "|<existing-system-name>\n";
    };
    const auto nargs = args.subspan(1u);
    for (auto&& arg: nargs) {
        if (arg == help_argument) {
            std::cout << "runs specified system definition in foreground.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
    }
    const auto& custom = std::get<flow::custom>(context.implementation);
    const auto found = find(custom.nodes, nargs[0].c_str());
    if (!found) {
        std::cerr << "no such system as ";
        std::cerr << std::quoted(nargs[0]);
        std::cerr << '\n';
        return;
    }
    foreground(nargs[0], update(*found, nargs), {
        .ports = context.interface,
        .environment = custom.environment
    });
}

auto do_rename(flow::node& context, const string_span& args) -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " ";
        os << help_argument;
        os << "|";
        os << usage_argument;
        os << "|<old-system-name> <new-system-name>\n";
    };
    const auto nargs = args.subspan(1u);
    for (auto&& arg: nargs) {
        if (arg == help_argument) {
            std::cout << "renames specified system definition to new name.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
    }
    if (size(nargs) != 2u) {
        std::cerr << "usage: ";
        std::cerr << args[0];
        std::cerr << " [" << help_argument;
        std::cerr << "] <old-name> <new-name>\n";
        return;
    }
    auto old_name = flow::system_name{};
    try {
        old_name = flow::system_name{nargs[0]};
    }
    catch (const flow::charset_validator_error& ex) {
        std::cerr << "old system name ";
        std::cerr << std::quoted(nargs[0]);
        std::cerr << " invalid: " << ex.what();
        return;
    }
    auto new_name = flow::system_name{};
    try {
        new_name = flow::system_name{nargs[1]};
    }
    catch (const flow::charset_validator_error& ex) {
        std::cerr << "new system name ";
        std::cerr << std::quoted(nargs[1]);
        std::cerr << " invalid: " << ex.what();
        return;
    }
    auto& custom = std::get<flow::custom>(context.implementation);
    auto subsystems = custom.nodes;
    auto nh = subsystems.extract(old_name);
    if (nh.empty()) {
        std::cerr << "no such subsystem as ";
        std::cerr << old_name;
        std::cerr << '\n';
        return;
    }
    nh.key() = new_name;
    const auto result = subsystems.insert(std::move(nh));
    if (!result.inserted) {
        std::cerr << "unable to rename system to ";
        std::cerr << new_name;
        std::cerr << "\n";
        return;
    }
    custom.nodes = std::move(subsystems);
}

auto do_set_system(flow::node& context, const string_span& args) -> void
{
    auto system = flow::node{};
    auto parent = std::string{};
    auto names = std::vector<std::string>{};
    auto custom = false;
    auto executable = false;
    auto file = std::string{};
    auto port_map_entries = std::vector<flow::port_map_entry>{};
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " ";
        os << help_argument << "|" << usage_argument;
        os << "| [" << parent_prefix << "<parent-name>] ";
        os << "<name>...";
        os << " [" << des_prefix << "<n>=<in|out>[:<comment>]...]";
        os << " [{} | " << file_prefix << "<file>" << " -- arg...]\n";
    };
    auto abort = [](std::ostream& os,
                    const std::string& within,
                    const std::string& msg){
        os << "aborting: unable to set systems within ";
        os << std::quoted(within);
        os << ": " << msg;
        os << "\n";
    };
    auto index = 1u;
    const auto custom_token = std::string{custom_begin_token}
                            + std::string{custom_end_token};
    for (auto&& arg: args.subspan(1u)) {
        ++index;
        if (arg == help_argument) {
            std::cout << "adds a new custom or executable system definition.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg.starts_with(parent_prefix)) {
            parent = arg.substr(parent_prefix.size());
            continue;
        }
        if (arg.starts_with(des_prefix)) {
            const auto value = arg.substr(size(des_prefix));
            const auto entry = parse_port_map_entry(value);
            if (!entry) {
                std::ostringstream os;
                os << "bad descriptor map entry: ";
                os << std::quoted(value);
                abort(std::cerr, parent, os.str());
                return;
            }
            port_map_entries.emplace_back(*entry);
            continue;
        }
        if (arg.starts_with(file_prefix)) {
            if (custom) {
                std::ostringstream os;
                os << std::quoted(arg);
                os << " for executable systems, not custom";
                abort(std::cerr, parent, os.str());
                return;
            }
            file = arg.substr(file_prefix.length());
            executable = true;
            continue;
        }
        if (arg == "--") {
            if (custom) {
                std::ostringstream os;
                os << std::quoted(arg);
                os << " for executable systems, not custom";
                abort(std::cerr, parent, os.str());
                return;
            }
            executable = true;
            // arguments after this are for flow::executable::arguments
            break;
        }
        if (arg == custom_token) {
            if (executable) {
                std::ostringstream os;
                os << std::quoted(arg);
                os << " for custom systems, not executable";
                abort(std::cerr, parent, os.str());
                return;
            }
            custom = true;
            continue;
        }
        if (arg.starts_with("-") || arg.starts_with("--")) {
            std::ostringstream os;
            os << "unrecognized argument " << std::quoted(arg);
            abort(std::cerr, parent, os.str());
            return;
        }
        names.emplace_back(arg);
    }
    if (empty(names)) {
        std::cerr << "aborting: one or more names must be specified\n";
        return;
    }
    auto parent_names = std::deque<flow::system_name>{};
    try {
        parent_names = flow::to_system_names(parent);
    }
    catch (const flow::charset_validator_error& ex) {
        std::cerr << "aborting: parent name(s) ";
        std::cerr << std::quoted(parent);
        std::cerr << " invalid: " << ex.what();
        std::cerr << "\n";
        return;
    }
    const auto parent_basis = parse({{}, parent_names, &context});
    if (!empty(parent) && !empty(parent_basis.remaining)) {
        abort(std::cerr, parent, "no such parent");
        return;
    }
    for (auto&& name: names) {
        auto base_names = std::deque<flow::system_name>{};
        try {
            base_names = flow::to_system_names(name);
        }
        catch (const flow::charset_validator_error& ex) {
            std::cerr << "skipping invalid system name(s) ";
            std::cerr << std::quoted(name);
            std::cerr << ": " << ex.what();
            std::cerr << "\n";
            continue;
        }
        const auto name_basis = parse({
            parent_basis.names, base_names, parent_basis.psystem
        }, 1u);
        switch (name_basis.remaining.size()) {
        case 1u:
            break;
        default:
            abort(std::cerr, parent, "no such parent");
            return;
        }
        const auto psys =
        std::get_if<flow::custom>(&(name_basis.psystem->implementation));
        if (!psys) {
            abort(std::cerr, parent, "parent not custom");
            return;
        }
        const auto arg_span = args.subspan(index);
        const auto arguments = std::vector(begin(arg_span), end(arg_span));
        if (!empty(file) || !empty(arguments)) {
            system.implementation = flow::executable{
                .file = file,
                .arguments = arguments
            };
            system.interface = flow::std_ports;
        }
        else {
            system.implementation = flow::custom{
                .environment = psys->environment
            };
            system.interface = name_basis.psystem->interface;
        }
        for (auto&& entry: port_map_entries) {
            update(system.interface, entry);
        }
        psys->nodes.insert_or_assign(name_basis.remaining.front(), system);
    }
}

auto do_show_connections(const flow::node& context, const string_span& args)
    -> void
{
    if (!empty(args)) {
        for (auto&& arg: args.subspan(1u)) {
            if (arg == help_argument) {
                std::cout << "shows connections within a system.\n";
                return;
            }
            if (arg == usage_argument) {
                std::cout << "usage: ";
                std::cout << args[0];
                std::cout << " [";
                std::cout << help_argument;
                std::cout << "|";
                std::cout << usage_argument;
                std::cout << "]\n";
                return;
            }
        }
    }
    const auto& custom = std::get<flow::custom>(context.implementation);
    for (auto&& c: custom.links) {
        if (const auto p = std::get_if<flow::unidirectional_connection>(&c)) {
            std::cout << p->src;
            std::cout << connection_separator;
            std::cout << p->dst;
            std::cout << '\n';
        }
    }
}

auto do_remove_connections(flow::node& context, const string_span& args)
    -> void
{
    auto src_str = std::string{};
    auto dst_str = std::string{};
    auto nconnects = 0;
    const auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " ";
        os << help_argument << "|" << usage_argument << "|";
        os << "<lhs_endpoint>-<rhs_endpoint>...";
        os << '\n';
    };
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "removes connections between endpoints within a system.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg.starts_with(src_prefix)) {
            src_str = arg.substr(src_prefix.size());
            continue;
        }
        if (arg.starts_with(dst_prefix)) {
            dst_str = arg.substr(dst_prefix.size());
            continue;
        }
        if (arg.starts_with("-")) {
            std::cerr << std::quoted(arg) << ": unrecognized argument.\n";
            continue;
        }
        const auto found = arg.find(connection_separator);
        if (found == arg.npos) {
            std::cerr << std::quoted(arg) << ": unrecognized argument.\n";
            continue;
        }
        const auto lhs = arg.substr(0u, found);
        const auto rhs = arg.substr(found + 1u);
        if (empty(lhs)) {
            std::cerr << std::quoted(arg);
            std::cerr << ": left-hand-side endpoint must be specified\n";
            continue;
        }
        if (empty(rhs)) {
            std::cerr << std::quoted(arg);
            std::cerr << ": right-hand-side endpoint must be specified\n";
            continue;
        }
        auto lhs_endpoint = flow::endpoint{};
        if (!parse(lhs_endpoint, lhs)) {
            std::cerr << std::quoted(lhs);
            std::cerr << ": can't parse left-hand-side endpoint\n";
            continue;
        }
        auto rhs_endpoint = flow::endpoint{};
        if (!parse(rhs_endpoint, rhs)) {
            std::cerr << std::quoted(rhs);
            std::cerr << ": can't parse right-hand-side endpoint\n";
            continue;
        }
        auto& custom = std::get<flow::custom>(context.implementation);
        const auto conn = flow::unidirectional_connection{
            lhs_endpoint, rhs_endpoint
        };
        std::cout << std::quoted(arg);
        std::cout << ": found and removed ";
        std::cout << erase(custom.links, flow::connection{conn});
        std::cout << " matching connection(s)\n";
        ++nconnects;
    }
    if (empty(src_str) && empty(dst_str)) {
        if (nconnects < 1) {
            usage(std::cerr);
        }
        return;
    }
    auto abort = [](std::ostream& os,
                    const std::string& src,
                    const std::string& dst,
                    const std::string& msg = {}){
        os << "aborting: unable to remove connection from ";
        os << std::quoted(src);
        os << " to ";
        os << std::quoted(dst);
        os << ": " << msg;
        os << "\n";
    };
    if (empty(src_str)) {
        abort(std::cerr, src_str, dst_str, "source must be specified");
        return;
    }
    if (empty(dst_str)) {
        abort(std::cerr, src_str, dst_str, "destination must be specified");
        return;
    }
    auto src_endpoint = flow::endpoint{};
    if (!parse(src_endpoint, src_str)) {
        abort(std::cerr, src_str, dst_str, "can't parse source");
        return;
    }
    auto dst_endpoint = flow::endpoint{};
    if (!parse(dst_endpoint, dst_str)) {
        abort(std::cerr, src_str, dst_str, "can't parse destination");
        return;
    }
    auto& custom = std::get<flow::custom>(context.implementation);
    const auto key = flow::connection{
        flow::unidirectional_connection{src_endpoint, dst_endpoint}
    };
    std::cout << "found and removed ";
    std::cout << erase(custom.links, key);
    std::cout << " matching connection(s)\n";
}

auto do_add_connections(flow::node& context, const string_span& args) -> void
{
    auto name = std::string{};
    const auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " ";
        os << help_argument << "|" << usage_argument << "|";
        os << "[" << parent_prefix << "<parent>]";
        os << "[" << name_prefix << "<name>]";
        os << " <lhs_endpoint>-<rhs_endpoint>...";
        os << '\n';
        os << "  where <lhs_endpoint> and <rhs_endpoint> are one of:\n";
        os << "  <user_endpoint>|<file_endpoint>|<system_endpoint>\n";
        os << "  where <user_endpoint> is: ";
        os << flow::reserved::user_endpoint_prefix;
        os << "<user-endpoint-name>\n";
        os << "  where <file_endpoint> is: ";
        os << flow::reserved::file_endpoint_prefix;
        os << "<file-system-path>\n";
        os << "  where <system_endpoint> is: ";
        os << flow::reserved::descriptors_prefix;
        os << "<number>[";
        os << flow::reserved::descriptor_separator;
        os << "<number>...][";
        os << flow::reserved::address_prefix;
        os << "<system-name>]\n";
    };
    auto parent_basis = system_basis{{}, {}, &context};
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "adds connections between endpoints within a system.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg.starts_with(parent_prefix)) {
            const auto parent = arg.substr(parent_prefix.size());
            auto parent_names = std::deque<flow::system_name>{};
            try {
                parent_names = flow::to_system_names(parent);
            }
            catch (const flow::charset_validator_error& ex) {
                std::cerr << "can't parse parent name " << std::quoted(parent);
                std::cerr << ": " << ex.what() << "\n";
                return;
            }
            parent_basis = parse({{}, parent_names, &context});
            if (!empty(parent) && !empty(parent_basis.remaining)) {
                std::cerr << std::quoted(parent) << "no such parent\n";
                return;
            }
            continue;
        }
        if (arg.starts_with(name_prefix)) {
            name = arg.substr(name_prefix.size());
            continue;
        }
        if (arg.starts_with("-")) {
            std::cerr << std::quoted(arg) << ": unrecognized argument.\n";
            continue;
        }
        const auto found = arg.find(connection_separator);
        if (found == arg.npos) {
            std::cerr << std::quoted(arg) << ": unrecognized argument.\n";
            continue;
        }
        const auto lhs = arg.substr(0u, found);
        const auto rhs = arg.substr(found + 1u);
        if (empty(lhs)) {
            std::cerr << std::quoted(arg);
            std::cerr << ": left-hand-side endpoint must be specified\n";
            continue;
        }
        if (empty(rhs)) {
            std::cerr << std::quoted(arg);
            std::cerr << ": right-hand-side endpoint must be specified\n";
            continue;
        }
        auto names = std::deque<flow::system_name>();
        try {
            names = flow::to_system_names(name);
        }
        catch (const flow::charset_validator_error& ex) {
            std::cerr << "can't parse name " << std::quoted(name);
            std::cerr << ": " << ex.what() << "\n";
            return;
        }
        const auto name_basis = parse({
            parent_basis.names, names, parent_basis.psystem
        });
        switch (name_basis.remaining.size()) {
        case 0u:
            break;
        default:
            std::cerr << std::quoted(name) << "no such system\n";
            return;
        }
        const auto p = std::get_if<flow::custom>(&name_basis.psystem->implementation);
        if (!p) {
            std::cerr << "specified containing system is not custom\n";
            return;
        }
        auto lhs_endpoint = flow::endpoint{};
        if (!parse(lhs_endpoint, lhs)) {
            std::cerr << std::quoted(lhs);
            std::cerr << ": can't parse left-hand-side endpoint\n";
            continue;
        }
        auto rhs_endpoint = flow::endpoint{};
        if (!parse(rhs_endpoint, rhs)) {
            std::cerr << std::quoted(rhs);
            std::cerr << ": can't parse right-hand-side endpoint\n";
            continue;
        }
        p->links.emplace_back(flow::unidirectional_connection{
            lhs_endpoint, rhs_endpoint
        });
    }
}

auto print(std::ostream& os, const flow::port_map& ports) -> void
{
    for (auto&& entry: ports) {
        os << ' ';
        os << des_prefix;
        os << entry.first;
        os << assignment_token;
        os << entry.second.direction;
        if (!empty(entry.second.comment)) {
            os << ':';
            os << std::quoted(entry.second.comment);
        }
    }
}

auto do_show_systems(const flow::node& context, const string_span& args) -> void
{
    constexpr auto show_info_argument = "--show-info";
    constexpr auto recursive_argument = "--recursive";
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " [";
        os << help_argument;
        os << "|";
        os << usage_argument;
        os << "|";
        os << "[";
        os << show_info_argument;
        os << "][";
        os << recursive_argument;
        os << "]\n";
    };
    auto show_info = true;
    auto show_recursive = false;
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "shows information about systems that have been added.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg == show_info_argument) {
            show_info = !show_info;
            continue;
        }
        if (arg == recursive_argument) {
            show_recursive = !show_recursive;
            continue;
        }
    }
    const auto& custom = std::get<flow::custom>(context.implementation);
    for (auto&& entry: custom.nodes) {
        std::cout << entry.first;
        print(std::cout, entry.second.interface);
        if (show_info) {
            if (const auto p = std::get_if<flow::executable>(&entry.second.implementation)) {
                if (!p->file.empty()) {
                    std::cout << ' ';
                    std::cout << file_prefix;
                    std::cout << p->file;
                }
                if (!empty(p->arguments)) {
                    std::cout << " --";
                    for (auto&& arg: p->arguments) {
                        std::cout << ' ';
                        std::cout << arg;
                    }
                }
            }
            else if (const auto p = std::get_if<flow::custom>(&entry.second.implementation)) {
                std::cout << ' ';
                std::cout << custom_begin_token;
                if (show_recursive && !empty(p->nodes)) {
                    std::cout << '\n';
                    auto opts = flow::detail::indenting_ostreambuf_options{
                        .indent = 2
                    };
                    const flow::detail::indenting_ostreambuf indent{std::cout, opts};
                    do_show_systems(entry.second, args);
                }
                std::cout << custom_end_token;
            }
        }
        std::cout << '\n';
    }
}

auto do_wait(flow::instance& instance, const string_span& args) -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << ' ';
        os << help_argument;
        os << '|';
        os << usage_argument;
        os << "|<instance-name>...\n";
    };
    auto& instances = std::get<flow::instance::custom>(instance.info).children;
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "waits for an instance.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        auto name = flow::system_name{};
        try {
            name = flow::system_name{arg};
        }
        catch (const flow::charset_validator_error& ex) {
            std::cerr << std::quoted(arg);
            std::cerr << ": not a valid system name, skipping.";
            continue;
        }
        const auto it = instances.find(name);
        if (it == instances.end()) {
            std::cerr << "no such instance as ";
            std::cerr << name;
            std::cerr << "\n";
            continue;
        }
        const auto results = wait(it->second);
        print(results, true);
        write_diags(it->second, std::cerr, arg);
        instances.erase(it);
    }
}

auto do_show_instances(flow::instance& instance, const string_span& args)
    -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " [";
        os << help_argument;
        os << '|';
        os << usage_argument;
        os << "\n";
    };
    const auto& custom = std::get<flow::instance::custom>(instance.info);
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "shows a listing of instantiations.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
    }
    if (empty(custom.children)) {
        std::cout << "no instances.\n";
    }
    else {
        std::cout << size(custom.children) << " instances:\n";
        const auto opts = flow::detail::indenting_ostreambuf_options{
            .indent = 2
        };
        const flow::detail::indenting_ostreambuf indent{std::cout, opts};
        for (auto&& entry: custom.children) {
            std::cout << entry.first << assignment_token << entry.second << "\n";
        }
    }
    if (empty(custom.channels)) {
        std::cout << "no channels.\n";
    }
    else {
        std::cout << size(custom.channels) << " channels:\n";
        const auto opts = flow::detail::indenting_ostreambuf_options{
            .indent = 2
        };
        const flow::detail::indenting_ostreambuf indent{std::cout, opts};
        for (auto&& channel: custom.channels) {
            std::cout << channel << "\n";
        }
    }
}

auto do_env(const flow::node& context, const string_span& args) -> void
{
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "prints the current environment variables.\n";
            return;
        }
        if (arg == usage_argument) {
            std::cout << "usage: ";
            std::cout << args[0];
            std::cout << " [";
            std::cout << help_argument;
            std::cout << "|";
            std::cout << usage_argument;
            std::cout << "]\n";
            return;
        }
    }
    auto& map = std::get<flow::custom>(context.implementation).environment;
    flow::pretty_print(std::cout, map, "\n");
    std::cout.flush();
}

auto do_setenv(flow::node& context, const string_span& args) -> void
{
    auto& map = std::get<flow::custom>(context.implementation).environment;
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " [--<flag>] <env-name>=<env-value>...\n";
        os << "  where <floag> may be:\n";
        os << "  ";
        os << usage_argument;
        os << ": shows this usage.\n";
        os << "  ";
        os << help_argument;
        os << ": shows help on this command.\n";
        os << "  --reset: resets the flow environment to given.\n";
    };
    for (auto&& arg: args.subspan(1u)) {
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg == help_argument) {
            std::cout << "sets the named environment variable ";
            std::cout << "to the given value.\n";
            return;
        }
        if (arg == "--reset") {
            map = flow::get_environ();
            return;
        }
        if (const auto found = parse_assignment(arg)) {
            map[found->first] = found->second;
            continue;
        }
    }
}

auto do_unsetenv(flow::node& context, const string_span& args) -> void
{
    constexpr auto all_argument = "--all";
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << ' ';
        os << help_argument;
        os << '|';
        os << usage_argument;
        os << '|';
        os << all_argument;
        os << '|';
        os << "<environment-variable-name>...";
        os << '\n';
    };
    auto& map = std::get<flow::custom>(context.implementation).environment;
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "unsets environment variables.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg == all_argument) {
            map.clear();
            continue;
        }
        if (map.erase(arg) == 0) {
            std::cerr << "no such environment variable as ";
            std::cerr << std::quoted(arg);
            std::cerr << "\n" << std::flush;
        }
    }
}

auto do_ports(flow::port_map& map, const string_span& args) -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " [";
        os << help_argument;
        os << '|';
        os << usage_argument;
        os << "]\n";
    };
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "prints the I/O ports of the current custom system.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
    }
    print(std::cout, map);
    std::cout << '\n';
}

auto do_history(history_ptr& hist, int& hist_size, const string_span& args)
    -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " [";
        os << help_argument;
        os << '|';
        os << usage_argument;
        os << "]\n";
    };
    HistEvent ev{};
    for (auto&& arg: args) {
        if (arg == help_argument) {
            std::cout << "shows the history of commands entered.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg == "clear") {
            history(hist.get(), &ev, H_CLEAR);
            return;
        }
    }
    const auto width = static_cast<int>(std::to_string(hist_size).size());
    for (auto rv = history(hist.get(), &ev, H_LAST);
         rv != -1;
         rv = history(hist.get(), &ev, H_PREV)) {
         std::cout << std::setw(width) << ev.num << " " << ev.str;
    }
}

auto do_editor(edit_line_ptr& el, const string_span& args) -> void
{
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "shows or sets the shell editor.\n";
            return;
        }
        if (arg == usage_argument) {
            std::cout << "usage: ";
            std::cout << args[0];
            std::cout << ' ';
            std::cout << '[';
            std::cout << help_argument;
            std::cout << '|';
            std::cout << usage_argument;
            std::cout << '|';
            std::cout << vi_editor_str;
            std::cout << '|';
            std::cout << emacs_editor_str;
            std::cout << ']';
            std::cout << "\n";
            return;
        }
        if ((arg == vi_editor_str) || (arg == emacs_editor_str)) {
            el_set(el.get(), EL_EDITOR, arg.c_str());
            continue;
        }
    }
    if (args.size() == 1u) {
        auto ptr = static_cast<const char *>(nullptr);
        el_get(el.get(), EL_EDITOR, &ptr);
        if (!ptr) {
            std::cerr << "unable to get current shell editor\n";
            return;
        }
        std::cout << "shell editor is currently ";
        std::cout << std::quoted(ptr);
        std::cout << '\n';
    }
}

auto do_help(const cmd_table& cmds, const string_span& args) -> void
{
    using strings = std::vector<std::string>;
    if (size(args) > 1u) {
        for (auto&& arg: args.subspan(1u)) {
            if (arg == help_argument) {
                std::cout << "provides help on builtin flow commands.\n";
                return;
            }
            if (arg == usage_argument) {
                std::cout << "usage: ";
                std::cout << args[0];
                std::cout << ' ';
                std::cout << help_argument;
                std::cout << '|';
                std::cout << usage_argument;
                std::cout << '|';
                std::cout << "<builtin-command-name>...\n";
                return;
            }
            const auto found = cmds.find(arg);
            if (found == cmds.end()) {
                std::cerr << std::quoted(arg);
                std::cerr << ": unknown command, skipping\n";
                continue;
            }
            std::cout << found->first;
            std::cout << ": ";
            const auto cargs = strings{found->first, help_argument};
            found->second(cargs);
        }
        return;
    }
    auto default_hash_code = std::size_t{};
    for (auto&& entry: cmds) {
        if (empty(entry.first)) {
            default_hash_code = entry.second.target_type().hash_code();
            continue;
        }
        std::cout << entry.first;
        if (entry.second.target_type().hash_code() == default_hash_code) {
            std::cout << " (default)";
        }
        std::cout << ": ";
        const auto cargs = strings{entry.first, help_argument};
        entry.second(cargs);
    }
}

auto do_usage(const cmd_table& cmds, const string_span& args) -> void
{
    using strings = std::vector<std::string>;
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << ' ';
        os << help_argument << "|";
        os << usage_argument << "|";
        os << "<builtin-command-name>...\n";
    };
    if (size(args) > 1u) {
        for (auto&& arg: args.subspan(1u)) {
            if (arg == help_argument) {
                std::cout << "provides usage syntax of builtin flow commands.\n";
                return;
            }
            if (arg == usage_argument) {
                usage(std::cout);
                return;
            }
            const auto found = cmds.find(arg);
            if (found == cmds.end()) {
                std::cerr << std::quoted(arg);
                std::cerr << ": unknown command, skipping\n";
                continue;
            }
            std::cout << found->first;
            std::cout << ' ';
            found->second(strings{found->first, usage_argument});
        }
        return;
    }
    if (!empty(args)) {
        usage(std::cout);
        return;
    }
    for (auto&& entry: cmds) {
        if (empty(entry.first)) {
            continue;
        }
        std::cout << entry.first;
        std::cout << ' ';
        entry.second(strings{entry.first, usage_argument});
    }
}

auto do_chdir(flow::node& context, const string_span& args) -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " ";
        os << help_argument;
        os << "|";
        os << usage_argument;
        os << "|";
        os << "<directory>";
        os << "\n";
    };
    auto& map = std::get<flow::custom>(context.implementation).environment;
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "changes the current working directory.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
    }
    if (args.size() != 2u) {
        std::cerr << "specify new working directory and only that\n";
        usage(std::cerr);
        return;
    }
    auto ec = std::error_code{};
    std::filesystem::current_path(args[1], ec);
    if (ec) {
        std::cerr << "cd ";
        std::cerr << std::quoted(args[1]);
        std::cerr << " failed: ";
        std::cerr << ec.message();
        std::cerr << "\n";
        return;
    }
    map["PWD"] = args[1];
}

auto do_push(system_stack_type& stack, const string_span& args) -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " ";
        os << help_argument;
        os << "|";
        os << usage_argument;
        os << "|";
        os << "<system-name>";
        os << "\n";
    };
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "pushes specified custom system onto stack.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
    }
    if (args.size() != 2u) {
        std::cerr << "invalid argument count " << args.size();
        std::cerr << ": specify custom system and only that\n";
        return;
    }
    auto names = std::deque<flow::system_name>();
    try {
        names = flow::to_system_names(args[1]);
    }
    catch (const flow::charset_validator_error& ex) {
        std::cerr << std::quoted(args[1]);
        std::cerr << " not sequence of valid system names: ";
        std::cerr << ex.what();
        std::cerr << "\n";
        return;
    }
    const auto name_basis = parse({{}, names, &stack.top().get()});
    if (!empty(name_basis.remaining)) {
        std::cerr << "unable to parse entire sequence of system names\n";
        return;
    }
    if (!std::holds_alternative<flow::custom>(name_basis.psystem->implementation)) {
        std::cerr << std::quoted(args[1]);
        std::cerr << ": not custom system, can only push into custom system\n";
        return;
    }
    stack.push(*name_basis.psystem);
}

auto do_pop(system_stack_type& stack, const string_span& args) -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " [";
        os << help_argument;
        os << "|";
        os << usage_argument;
        os << "|";
        os << rebase_prefix << "<new-system-name>";
        os << "]\n";
    };
    auto rebase = false;
    auto rebase_name = std::string{};
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "pops the current custom system off the stack.\n";
            return;
        }
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg.starts_with(rebase_prefix)) {
            rebase_name = arg.substr(rebase_prefix.size());
            rebase = true;
            continue;
        }
        if (arg.starts_with("-")) {
            std::cerr << std::quoted(arg) << ": unrecognized argument.\n";
            continue;
        }
    }
    if (stack.size() > 1u) {
        stack.pop();
        return;
    }
    if (!rebase) {
        std::cerr << "already at root custom system";
        std::cerr << " and rebase not specified.\n";
        return;
    }
    const auto copy_of_top = stack.top().get();
    stack.top().get() = flow::custom{
        .nodes = {{rebase_name, copy_of_top}}
    };
}

auto run(const cmd_handler& cmd, const string_span& args) -> void
{
    try {
        cmd(args);
    }
    catch (const std::invalid_argument& ex) {
        std::cerr << "exception caught from running ";
        std::cerr << args[0];
        std::cerr << " command: ";
        std::cerr << ex.what();
        std::cerr << "\n";
    }
    catch (...) {
        std::cerr << "exception caught from running ";
        std::cerr << args[0];
        std::cerr << " command\n";
    }
}

auto do_cmds(const cmd_table& cmds, const string_span& args) -> void
{
    if (!empty(args)) {
        if (args[0] == help_argument) {
            std::cout << "\n";
            auto opts = flow::detail::indenting_ostreambuf_options{
                .indent = 2
            };
            const flow::detail::indenting_ostreambuf indent{std::cout, opts};
            do_help(cmds, args.subspan(1u));
            return;
        }
        if (args[0] == usage_argument) {
            std::cout << "\n";
            auto opts = flow::detail::indenting_ostreambuf_options{
                .indent = 2
            };
            const flow::detail::indenting_ostreambuf indent{std::cout, opts};
            do_usage(cmds, args.subspan(1u));
            return;
        }
    }
    const auto cmd = empty(args)? std::string{}: args[0];
    if (const auto it = cmds.find(cmd); it != cmds.end()) {
        const auto default_args = std::vector<std::string>{it->first};
        run(it->second, empty(args)? default_args: args);
    }
    else {
        std::cerr << std::quoted(cmd);
        std::cerr << ": no such command\n";
    }
}

}

auto main(int argc, const char * argv[]) -> int
{
    auto environment = flow::get_environ();
    environment["SHELL"] = argv[0];
    auto root_system = flow::node{
        flow::custom{environment},
        flow::std_ports,
    };
    system_stack_type system_stack;
    system_stack.push(root_system);
    auto instance = flow::instance{flow::instance::custom{}};
    auto do_loop = true;
    auto hist_size = 100;

    // For example of using libedit, see: https://tinyurl.com/3ez9utzc
    HistEvent ev{};
    auto hist = history_ptr{history_init()};
    history(hist.get(), &ev, H_SETSIZE, hist_size);

    auto tok = tokenizer_ptr{tok_init(NULL)};

    auto el = edit_line_ptr{el_init(argv[0], stdin, stdout, stderr)};
    el_set(el.get(), EL_SIGNAL, 1); // installs sig handlers for resizing, etc.
    el_set(el.get(), EL_HIST, history, hist.get());
    el_set(el.get(), EL_PROMPT_ESC, prompt, '\1');
    el_set(el.get(), EL_EDITOR, emacs_editor_str);
    el_source(el.get(), NULL);

    for (auto i = 0; i < argc; ++i) {
        const auto arg = std::string_view{argv[i]};
        if (arg.starts_with(des_prefix)) {
            if (const auto p =
                parse_port_map_entry(arg.substr(size(des_prefix)))) {
                update(system_stack.top().get().interface, *p);
            }
            continue;
        }
    }

    const auto show_sys_lambda = [&](const string_span& args){
        do_show_systems(system_stack.top().get(), args);
    };
    const cmd_table sys_cmds{
        {"", show_sys_lambda},
        {"rename", [&](const string_span& args){
            do_rename(system_stack.top().get(), args);
        }},
        {"run", [&](const string_span& args){
            do_foreground(system_stack.top().get(), args);
        }},
        {"set", [&](const string_span& args){
            do_set_system(system_stack.top().get(), args);
        }},
        {"show", show_sys_lambda},
        {"unset", [&](const string_span& args){
            do_unset_system(system_stack.top().get(), args);
        }},
    };

    const auto show_inst_lambda = [&](const string_span& args){
        do_show_instances(instance, args);
    };
    const cmd_table inst_cmds{
        {"", show_inst_lambda},
        {"show", show_inst_lambda},
        {"wait", [&](const string_span& args){
            do_wait(instance, args);
        }}
    };

    const auto show_conns_lambda = [&](const string_span& args){
        do_show_connections(system_stack.top().get(), args);
    };
    const cmd_table conn_cmds{
        {"", show_conns_lambda},
        {"add", [&](const string_span& args){
            do_add_connections(system_stack.top().get(), args);
        }},
        {"remove", [&](const string_span& args){
            do_remove_connections(system_stack.top().get(), args);
        }},
        {"show", show_conns_lambda},
    };

    const auto show_env_lambda = [&](const string_span& args){
        do_env(system_stack.top().get(), args);
    };
    const cmd_table env_cmds{
        {"", show_env_lambda},
        {"set", [&](const string_span& args){
            do_setenv(system_stack.top().get(), args);
        }},
        {"show", show_env_lambda},
        {"unset", [&](const string_span& args){
            do_unsetenv(system_stack.top().get(), args);
        }},
    };

    // TODO: make this into a table of CRUD commands...
    const cmd_table cmds{
        {"exit", [&](const string_span& args){
            for (auto&& arg: args.subspan(1u)) {
                if (arg == help_argument) {
                    std::cout << "exits this shell.\n";
                    return;
                }
            }
            do_loop = false;
        }},
        {"help", [&](const string_span& args){
            if (size(args) == 1u) {
                std::cout << "Builtin flow commands (and their sub-commands):\n\n";
            }
            do_help(cmds, args);
        }},
        {"editor", [&](const string_span& args){
            do_editor(el, args);
        }},
        {"history", [&](const string_span& args){
            do_history(hist, hist_size, args);
        }},
        {"ports", [&](const string_span& args){
            do_ports(system_stack.top().get().interface, args);
        }},
        {"cd", [&](const string_span& args){
            do_chdir(system_stack.top().get(), args);
        }},
        {"env", [&](const string_span& args){
            do_cmds(env_cmds, args.subspan(1u));
        }},
        {"instances", [&](const string_span& args){
            do_cmds(inst_cmds, args.subspan(1u));
        }},
        {"systems", [&](const string_span& args){
            do_cmds(sys_cmds, args.subspan(1u));
        }},
        {"connections", [&](const string_span& args){
            do_cmds(conn_cmds, args.subspan(1u));
        }},
        {"push", [&](const string_span& args){
            do_push(system_stack, args);
        }},
        {"pop", [&](const string_span& args){
            do_pop(system_stack, args);
        }},
        {"usage", [&](const string_span& args){
            do_usage(cmds, args);
        }},
    };

    while (do_loop) {
        auto count = 0;
        const auto buf = el_gets(el.get(), &count);
        if (!buf || count == 0) {
            const auto err = errno;
            if (errno == EINTR) {
                std::cerr << "el_gets was interrupted\n";
                continue;
            }
            std::cerr << "aborting: el_gets returned null, count=";
            std::cerr << count;
            std::cerr << ", errno=";
            std::cerr << flow::os_error_code(err);
            std::cerr << "\n";
            break;
        }
        if (!continuation && (count == 1)) {
            continue;
        }
        const auto li = el_line(el.get());
        auto ac = 0; // arg count
        auto av = static_cast<const char**>(nullptr);
        auto cc = 0;
        auto co = 0;
        const auto tok_line_rv = tok_line(tok.get(), li, &ac, &av, &cc, &co);
        if (tok_line_rv == -1) {
            std::cerr << "Internal error\n";
            continuation = false;
            continue;
        }
        const auto hist_rv = history(hist.get(), &ev,
                                     continuation? H_APPEND: H_ENTER, buf);
        if (hist_rv == -1) {
            std::cerr << "history error (" << ev.num << ")" << ev.str << "\n";
        }
        continuation = tok_line_rv > 0;
        if (continuation) {
            continue;
        }
        if (ac < 1) {
            std::cerr << "unexpected argument count of " << ac << "\n";
            continue;
        }
        if (!av) {
            std::cerr << "unexpected null argument vector\n";
            continue;
        }
        auto args = make_arguments(ac, av);
        const auto bg_requested = args.back() == background_argument;
        if (bg_requested) {
            args.pop_back();
        }
        if (const auto it = cmds.find(args[0]); it != cmds.end()) {
            run(it->second, args);
        }
        else if (const auto found = find(system_stack, args)) {
            const auto system_name = std::string{args[0]};
            auto& context = system_stack.top().get();
            const auto opts = get_instantiate_options(context);
            auto derived_name = system_name;
            const auto tsys = update(*found, args);
            if (tsys != *found) {
                auto& custom = std::get<flow::custom>(context.implementation);
                derived_name = bg_job_name(args[0]);
                custom.nodes.emplace(derived_name, tsys);
            }
            if (bg_requested) {
                if (auto obj = instantiate(derived_name, tsys, opts)) {
                    auto& ci = std::get<flow::instance::custom>(instance.info);
                    ci.children.emplace(derived_name, std::move(*obj));
                }
            }
            else {
                foreground(derived_name, tsys, opts);
            }
        }
        else if (el_parse(el.get(), ac, av) == -1) {
            std::cerr << "unrecognized command " << av[0] << "\n";
            std::cerr << "enter " << std::quoted("help") << " for help.\n";
        }
        tok_reset(tok.get());
    }
    return 0;
}
