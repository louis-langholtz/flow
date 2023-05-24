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
#include "flow/connection.hpp"
#include "flow/reference_descriptor.hpp"
#include "flow/environment_map.hpp"
#include "flow/indenting_ostreambuf.hpp"
#include "flow/instantiate.hpp"
#include "flow/utility.hpp"

namespace {

using arguments = std::vector<std::string>;
using string_span = std::span<const std::string>;
using system_stack_type = std::stack<std::reference_wrapper<flow::system>>;

using cmd_handler = std::function<void(const string_span& args)>;
using cmd_table = std::map<std::string, cmd_handler>;

constexpr auto shell_name = "flow";

const auto des_prefix = std::string{"--des-"};
const auto name_prefix = std::string{"--name="};
const auto parent_prefix = std::string{"--parent="};
const auto file_prefix = std::string{"--file="};
const auto help_argument = std::string{"--help"};
const auto usage_argument = std::string{"--usage"};

auto make_arguments(int ac, const char*av[]) -> arguments
{
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

auto parse_descriptor_map_entry(const std::string_view& arg)
    -> std::optional<flow::descriptor_map_entry>
{
    const auto found_eq = arg.find('=');
    if (found_eq == std::string_view::npos) {
        std::cerr << "aborting: no '=' found in ";
        std::cerr << "descriptor map entry argument\n";
        return {};
    }
    const auto key = arg.substr(0u, found_eq);
    const auto found_int = to_int(key);
    if (!found_int) {
        std::cerr << "aborting: invalid key ";
        std::cerr << std::quoted(key);
        std::cerr << "\n";
        return {};
    }
    auto dir_comp = std::string_view{};
    auto des_info = flow::descriptor_info{};
    const auto value = arg.substr(found_eq + 1u);
    if (const auto found = value.find(':');
        found != std::string::npos) {
        des_info.comment = value.substr(found + 1u);
        dir_comp = value.substr(0, found);
    }
    else {
        dir_comp = value;
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
    return {flow::descriptor_map_entry{
        flow::reference_descriptor(*found_int), des_info
    }};
}

auto update(flow::descriptor_map& map, const flow::descriptor_map_entry& entry)
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

    /// @brief Remaining system names that have yet to be parsed through
    ///   possibly because no matches for them have been found yet.
    std::deque<flow::system_name> remaining;

    /// @brief Pointer to map of systems arrived at from @root.
    flow::system *psystem;

    auto operator==(const system_basis& other) const noexcept -> bool = default;
};

auto parse(system_basis from, std::size_t n_remain = 0u) -> system_basis
{
    while (size(from.remaining) > n_remain) {
        const auto p = std::get_if<flow::system::custom>(&from.psystem->info);
        if (!p) {
            break;
        }
        const auto found = p->subsystems.find(from.remaining.front());
        if (found == p->subsystems.end()) {
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

auto do_remove_system(flow::system& context, const string_span& args) -> void
{
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "removes system definitions.\n";
            return;
        }
        auto sys_names = std::deque<flow::system_name>{};
        try {
            sys_names = flow::to_system_names(arg);
        }
        catch (const std::invalid_argument& ex) {
            std::cerr << "invalid name ";
            std::cerr << arg;
            std::cerr << ": ";
            std::cerr << ex.what();
            std::cerr << "\n";
            continue;
        }
        const auto arg_basis = parse({{}, sys_names, &context}, 1u);
        const auto p =
            std::get_if<flow::system::custom>(&arg_basis.psystem->info);
        if (!p) {
            std::cerr << "system not custom?!\n";
            continue;
        }
        if (size(arg_basis.remaining) > 1u) {
            std::cerr << "no such system as " << arg << "\n";
            continue;
        }
        if (p->subsystems.erase(arg_basis.remaining.front()) == 0) {
            std::cerr << arg << " not found\n";
        }
    }
}

auto do_add_system(flow::system& context, const string_span& args) -> void
{
    auto system = flow::system{};
    auto parent = std::string{};
    auto name = std::string{};
    auto file = std::string{};
    auto descriptor_map_entries = std::vector<flow::descriptor_map_entry>{};
    auto usage = [&](std::ostream& os){
        os << "  usage: ";
        os << args[0];
        os << " ";
        os << help_argument << "|" << usage_argument;
        os << "| [" << parent_prefix << "<name>] ";
        os << name_prefix << "<name>";
        os << " [--des-<n>=<in|out>[:<comment>]]";
        os << " [" << file_prefix << "<file>" << " -- arg...]\n";
    };
    auto abort = [](std::ostream& os,
                    const std::string& key,
                    const std::string& within,
                    const std::string& msg){
        os << "aborting: unable to add system named ";
        os << std::quoted(key);
        os << " within ";
        os << std::quoted(within);
        os << ": " << msg;
        os << "\n";
    };
    auto index = 1u;
    for (auto&& arg: args.subspan(1u)) {
        ++index;
        if (arg == help_argument) {
            std::cout << "adds a new executable system definition.\n";
            usage(std::cout);
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
        if (arg.starts_with(name_prefix)) {
            name = arg.substr(name_prefix.length());
            continue;
        }
        if (arg.starts_with(des_prefix)) {
            const auto value = arg.substr(size(des_prefix));
            const auto entry = parse_descriptor_map_entry(value);
            if (!entry) {
                std::ostringstream os;
                os << "bad descriptor map entry: ";
                os << std::quoted(value);
                abort(std::cerr, name, parent, os.str());
                return;
            }
            descriptor_map_entries.emplace_back(*entry);
            continue;
        }
        if (arg.starts_with(file_prefix)) {
            file = arg.substr(file_prefix.length());
            continue;
        }
        if (arg == "--") {
            break;
        }
        std::ostringstream os;
        os << "unrecognized argument " << std::quoted(arg);
        abort(std::cerr, name, parent, os.str());
        return;
    }
    if (empty(name)) {
        std::cerr << "aborting: name must be specified\n";
        return;
    }
    auto parent_names = std::deque<flow::system_name>{};
    try {
        parent_names = flow::to_system_names(parent);
    }
    catch (const std::invalid_argument& ex) {
        std::ostringstream os;
        os << ex.what() << "\n";
        abort(std::cerr, name, parent, os.str());
        return;
    }
    const auto parent_basis = parse({{}, parent_names, &context});
    if (!empty(parent) && !empty(parent_basis.remaining)) {
        abort(std::cerr, name, parent, "no such parent");
        return;
    }
    auto names = std::deque<flow::system_name>{};
    try {
        names = flow::to_system_names(name);
    }
    catch (const std::invalid_argument& ex) {
        std::ostringstream os;
        os << ex.what() << "\n";
        abort(std::cerr, name, parent, os.str());
        return;
    }
    const auto name_basis = parse({
        parent_basis.names, names, parent_basis.psystem
    });
    switch (name_basis.remaining.size()) {
    case 0u:
        abort(std::cerr, name, parent, "already exists");
        return;
    case 1u:
        break;
    default:
        abort(std::cerr, name, parent, "no such parent");
        return;
    }
    const auto psys =
        std::get_if<flow::system::custom>(&(name_basis.psystem->info));
    if (!psys) {
        abort(std::cerr, name, parent, "parent not custom");
        return;
    }
    const auto arg_span = args.subspan(index);
    const auto arguments = std::vector(begin(arg_span), end(arg_span));
    if (!empty(file) || !empty(arguments)) {
        system.info = flow::system::executable{
            .file = file,
            .arguments = arguments
        };
        system.descriptors = flow::std_descriptors;
        for (auto&& entry: descriptor_map_entries) {
            update(system.descriptors, entry);
        }
    }
    if (!psys->subsystems.emplace(name_basis.remaining.front(), system).second) {
        abort(std::cerr, name, parent, "reason unknown");
        return;
    }
}

auto do_add_connection(flow::system& context, const string_span& args) -> void
{
    static const auto src_prefix = std::string{"--src="};
    static const auto dst_prefix = std::string{"--dst="};
    auto src_str = std::string{};
    auto dst_str = std::string{};
    auto name = std::string{};
    auto parent = std::string{};
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "adds connection between endpoints within a system.\n";
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
        if (arg.starts_with(name_prefix)) {
            name = arg.substr(name_prefix.size());
            continue;
        }
        if (arg.starts_with(parent_prefix)) {
            parent = arg.substr(parent_prefix.size());
            continue;
        }
    }
    auto abort = [](std::ostream& os,
                    const std::string& src,
                    const std::string& dst,
                    const std::string& msg = {}){
        os << "aborting: unable to add connection from ";
        os << std::quoted(src);
        os << " to ";
        os << std::quoted(dst);
        os << ": " << msg;
        os << "\n";
    };
    auto parent_names = std::deque<flow::system_name>{};
    try {
        parent_names = flow::to_system_names(parent);
    }
    catch (const std::invalid_argument& ex) {
        std::ostringstream os;
        os << "can't parse parent name " << parent;
        os << " - " << ex.what() << "\n";
        abort(std::cerr, src_str, dst_str, os.str());
        return;
    }
    const auto parent_basis = parse({{}, parent_names, &context});
    if (!empty(parent) && !empty(parent_basis.remaining)) {
        abort(std::cerr, src_str, dst_str, "no such parent");
        return;
    }
    auto names = std::deque<flow::system_name>();
    try {
        names = flow::to_system_names(name);
    }
    catch (const std::invalid_argument& ex) {
        std::ostringstream os;
        os << ex.what() << "\n";
        abort(std::cerr, name, parent, os.str());
        return;
    }
    const auto name_basis = parse({
        parent_basis.names, names, parent_basis.psystem
    });
    switch (name_basis.remaining.size()) {
    case 0u:
        break;
    default:
        abort(std::cerr, src_str, dst_str, "no such system");
        return;
    }
    const auto p = std::get_if<flow::system::custom>(&name_basis.psystem->info);
    if (!p) {
        abort(std::cerr, src_str, dst_str, "containing system not custom");
        return;
    }
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
    p->connections.emplace_back(flow::unidirectional_connection{
        src_endpoint, dst_endpoint
    });
}

auto do_show_system(flow::system& context, const string_span& args) -> void
{
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "shows the system definitions.\n";
            return;
        }
    }
    pretty_print(std::cout, context);
}

auto do_wait(flow::instance& instance, const string_span& args) -> void
{
    auto& instances = std::get<flow::instance::custom>(instance.info).children;
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "waits for an instance.\n";
            return;
        }
        const auto name = flow::system_name{arg};
        const auto it = instances.find(name);
        if (it == instances.end()) {
            std::cerr << "no such instance as ";
            std::cerr << name;
            std::cerr << "\n";
            continue;
        }
        const auto results = wait(it->second);
        for (auto&& result: results) {
            std::cout << result << "\n";
        }
        write_diags(it->second, std::cerr, arg);
        instances.erase(it);
    }
}

auto do_show_instances(flow::instance& instance, const string_span& args)
    -> void
{
    auto& instances = std::get<flow::instance::custom>(instance.info).children;
    for (auto&& arg: args) {
        if (arg == help_argument) {
            std::cout << "shows a listing of instantiations.\n";
            return;
        }
    }
    if (instances.empty()) {
        std::cout << "empty.\n";
        return;
    }
    for (auto&& entry: instances) {
        std::cout << entry.first << "=" << entry.second << "\n";
    }
}

auto do_env(const flow::environment_map& map, const string_span& args) -> void
{
    for (auto&& arg: args) {
        if (arg == help_argument) {
            std::cout << "prints the current environment variables\n";
            return;
        }
    }
    flow::pretty_print(std::cout, map, "\n");
    std::cout.flush();
}

auto do_setenv(flow::environment_map& map, const string_span& args) -> void
{
    auto usage = [&](std::ostream& os){
        os << "usage: ";
        os << args[0];
        os << " [--<flag>] [<env-name> <env-value>]\n";
        os << "  where <floag> may be:\n";
        os << "  ";
        os << usage_argument;
        os << ": shows this usage.\n";
        os << "  ";
        os << help_argument;
        os << ": shows help on this command.\n";
        os << "  --reset: resets the flow environment to given.\n";
    };
    auto argc = 0;
    for (auto&& arg: args) {
        if (arg == usage_argument) {
            usage(std::cout);
            return;
        }
        if (arg == help_argument) {
            std::cout << "sets the named environment variable ";
            std::cout << "to the given value\n";
            return;
        }
        if (arg == "--reset") {
            map = flow::get_environ();
            --argc;
            continue;
        }
    }
    switch (argc) {
    case 1:
        break;
    case 3:
        map[args[1]] = (args.size() > 2)? args[2]: "";
        break;
    default:
        usage(std::cerr);
        break;
    }
}

auto do_unsetenv(flow::environment_map& map, const string_span& args) -> void
{
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "unsets the named environment variables\n";
            return;
        }
        if (arg == "--all") {
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

auto do_descriptors(flow::descriptor_map& map, const string_span& args) -> void
{
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "prints the I/O descriptors table\n";
            return;
        }
    }
    std::cout << map;
    std::cout << "\n";
}

auto do_history(history_ptr& hist, int& hist_size, const string_span& args)
    -> void
{
    HistEvent ev{};
    for (auto&& arg: args) {
        if (arg == help_argument) {
            std::cout << "shows the history of commands entered\n";
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
            std::cout << "shows or sets the shell editor\n";
            return;
        }
        if (arg == "vi" || arg == "emacs") {
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
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "provides help on builtin flow commands\n";
            return;
        }
    }
    std::cout << "Builtin flow commands:\n";
    for (auto&& entry: cmds) {
        std::cout << entry.first << ": ";
        using strings = std::vector<std::string>;
        const auto cargs = strings{entry.first, help_argument};
        (cmds.at(entry.first))(cargs);
    }
}

auto do_chdir(flow::environment_map& map, const string_span& args) -> void
{
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "changes the current working directory\n";
            return;
        }
    }
    if (args.size() != 2u) {
        std::cerr << "specify new working directory and only that\n";
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
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "pushes specified custom system onto stack\n";
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
    catch (const std::invalid_argument& ex) {
        std::cerr << std::quoted(args[1]);
        std::cerr << " not sequence of valid system names:s ";
        std::cerr << ex.what();
        std::cerr << "\n";
        return;
    }
    const auto name_basis = parse({{}, names, &stack.top().get()});
    if (!empty(name_basis.remaining)) {
        std::cerr << "unable to parse entire sequence of system names\n";
        return;
    }
    if (!std::holds_alternative<flow::system::custom>(name_basis.psystem->info)) {
        std::cerr << std::quoted(args[1]);
        std::cerr << ": not custom system, can only push into custom system\n";
        return;
    }
    stack.push(*name_basis.psystem);
}

auto do_pop(system_stack_type& stack, const string_span& args) -> void
{
    for (auto&& arg: args.subspan(1u)) {
        if (arg == help_argument) {
            std::cout << "pops the current custom system off the stack\n";
            return;
        }
    }
    if (stack.size() == 1u) {
        std::cerr << "already at root custom system.\n";
        return;
    }
    stack.pop();
}

auto find(std::map<flow::system_name, flow::system>& map, const char* name)
    -> flow::system*
{
    auto sname = flow::system_name{};
    try {
        sname = flow::system_name{name};
    }
    catch (const std::invalid_argument& ex) {
        return nullptr;
    }
    const auto entry = map.find(sname);
    if (entry == map.end()) {
        return nullptr;
    }
    return &entry->second;
}

auto find(const system_stack_type& stack, const char* name) -> flow::system*
{
    auto& custom = std::get<flow::system::custom>(stack.top().get().info);
    return find(custom.subsystems, name);
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

}

auto main(int argc, const char * argv[]) -> int
{
    auto root_system = flow::system{
        flow::system::custom{}, flow::std_descriptors, flow::get_environ()
    };
    root_system.environment["SHELL"] = argv[0];
    system_stack_type system_stack;
    system_stack.push(root_system);
    auto instance = flow::instance{flow::instance::custom{}};
    flow::set_signal_handler(flow::signal::interrupt);
    flow::set_signal_handler(flow::signal::terminate);
    auto do_loop = true;
    auto hist_size = 100;
    auto sequence = std::size_t{0};

    // For example of using libedit, see: https://tinyurl.com/3ez9utzc
    HistEvent ev{};
    auto hist = history_ptr{history_init()};
    history(hist.get(), &ev, H_SETSIZE, hist_size);

    auto tok = tokenizer_ptr{tok_init(NULL)};

    auto el = edit_line_ptr{el_init(argv[0], stdin, stdout, stderr)};
    el_set(el.get(), EL_SIGNAL, 1); // installs sig handlers for resizing, etc.
    el_set(el.get(), EL_HIST, history, hist.get());
    el_set(el.get(), EL_PROMPT_ESC, prompt, '\1');
    el_source(el.get(), NULL);

    for (auto i = 0; i < argc; ++i) {
        const auto arg = std::string_view{argv[i]};
        if (arg.starts_with(des_prefix)) {
            if (const auto p =
                parse_descriptor_map_entry(arg.substr(size(des_prefix)))) {
                update(system_stack.top().get().descriptors, *p);
            }
            continue;
        }
    }

    // TODO: make this into a table of CRUD commands...
    const cmd_table cmds{
        {"exit", [&](const string_span& args){
            for (auto&& arg: args.subspan(1u)) {
                if (arg == help_argument) {
                    std::cout << "exits the shell\n";
                    return;
                }
            }
            do_loop = false;
        }},
        {"help", [&](const string_span& args){
            do_help(cmds, args);
        }},
        {"editor", [&](const string_span& args){
            do_editor(el, args);
        }},
        {"history", [&](const string_span& args){
            do_history(hist, hist_size, args);
        }},
        {"descriptors", [&](const string_span& args){
            do_descriptors(system_stack.top().get().descriptors, args);
        }},
        {"cd", [&](const string_span& args){
            do_chdir(system_stack.top().get().environment, args);
        }},
        {"env", [&](const string_span& args){
            do_env(system_stack.top().get().environment, args);
        }},
        {"setenv", [&](const string_span& args){
            do_setenv(system_stack.top().get().environment, args);
        }},
        {"unsetenv", [&](const string_span& args){
            do_unsetenv(system_stack.top().get().environment, args);
        }},
        {"show-system", [&](const string_span& args){
            do_show_system(system_stack.top().get(), args);
        }},
        {"show-instances", [&](const string_span& args){
            do_show_instances(instance, args);
        }},
        {"remove-system", [&](const string_span& args){
            do_remove_system(system_stack.top().get(), args);
        }},
        {"add-system", [&](const string_span& args){
            do_add_system(system_stack.top().get(), args);
        }},
        {"add-connection", [&](const string_span& args){
            do_add_connection(system_stack.top().get(), args);
        }},
        {"wait", [&](const string_span& args){
            do_wait(instance, args);
        }},
        {"push", [&](const string_span& args){
            do_push(system_stack, args);
        }},
        {"pop", [&](const string_span& args){
            do_pop(system_stack, args);
        }},
    };

    auto count = 0;
    auto buf = static_cast<const char*>(nullptr);
    while (do_loop && ((buf = el_gets(el.get(), &count)) != nullptr)) {
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
            std::cerr << "unexpexred argument count of " << ac << "\n";
            continue;
        }
        if (const auto it = cmds.find(av[0]); it != cmds.end()) {
            run(it->second, make_arguments(ac, av));
        }
        else if (const auto found = find(system_stack, av[0])) {
            auto tsys = *found;
            if (const auto p = std::get_if<flow::system::executable>(&tsys.info)) {
                if (ac > 1) {
                    std::copy(av, av + ac, std::back_inserter(p->arguments));
                }
            }
            const auto name = std::string{av[0]} + ((ac > 1)? "+": "-") +
                std::to_string(sequence);
            try {
                auto obj = instantiate(tsys, std::cerr, flow::instantiate_options{
                    .descriptors = system_stack.top().get().descriptors,
                    .environment = system_stack.top().get().environment
                });
                const auto results = wait(obj);
                for (auto&& result: results) {
                    std::cout << result << "\n";
                }
                write_diags(obj, std::cerr, name);
            }
            catch (const std::invalid_argument& ex) {
                std::cerr << "cannot instantiate ";
                std::cerr << std::quoted(av[0]);
                std::cerr << ": ";
                std::cerr << ex.what();
                std::cerr << "\n";
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
