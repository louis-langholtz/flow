#include <cstdlib> // for std::strtol
#include <functional> // for std::function
#include <iomanip> // for std::quoted
#include <iostream>
#include <iterator>
#include <iomanip> // for std::setw
#include <memory> // for std::unique_ptr
#include <ostream> // for std::flush
#include <span>
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

const auto des_prefix = std::string{"--des-"};
const auto name_prefix = std::string{"--name="};
const auto parent_prefix = std::string{"--parent="};
const auto help_argument = std::string{"--help"};

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

struct HistoryDeleter
{
    void operator()(History *p)
    {
        history_end(p);
    }
};

struct TokenizerDeleter
{
    void operator()(Tokenizer *p)
    {
        tok_end(p);
    }
};

auto continuation = false;

char *prompt([[maybe_unused]] EditLine *el)
{
    static char nl_buf[] = "\1\033[7m\1flow$\1\033[0m\1 ";
    static char cl_buf[] = "flow> ";
    return continuation? cl_buf: nl_buf;
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

auto parse(system_basis from) -> system_basis
{
    while (!empty(from.remaining)) {
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

}

auto main(int argc, const char * argv[]) -> int
{
    using arguments = std::vector<std::string>;
    using string_span = std::span<const std::string>;
    using cmd_handler = std::function<void(const string_span& args)>;
    using cmd_table = std::map<std::string, cmd_handler>;
    using edit_line_ptr = std::unique_ptr<EditLine, EditLineDeleter>;
    using history_ptr = std::unique_ptr<History, HistoryDeleter>;
    using tokenizer_ptr = std::unique_ptr<Tokenizer, TokenizerDeleter>;

    auto instantiate_opts = flow::instantiate_options{
        flow::std_descriptors, flow::get_environ()};
    auto system = flow::system{flow::system::custom{}};
    auto& custom = std::get<flow::system::custom>(system.info);
    auto instance = flow::instance{flow::instance::custom{}};
    auto& instances = std::get<flow::instance::custom>(instance.info).children;
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
            if (const auto p = parse_descriptor_map_entry({
                begin(arg) + size(des_prefix), end(arg)
            })) {
                if (p->second.direction == flow::io_type::none) {
                    instantiate_opts.descriptors.erase(p->first);
                }
                else if (const auto ret = instantiate_opts.descriptors.emplace(*p);
                    !ret.second) {
                    ret.first->second = p->second;
                }
            }
            continue;
        }
    }

    // TODO: make this into a table of CRUD commands...
    const cmd_table cmds{
        {"exit", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == help_argument) {
                    std::cout << "exits the shell\n";
                    return;
                }
            }
            do_loop = false;
        }},
        {"help", [&](const string_span& args){
            for (auto&& arg: args) {
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
        }},
        {"history", [&](const string_span& args){
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
        }},
        {"descriptors", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == help_argument) {
                    std::cout << "prints the descriptors table\n";
                    return;
                }
            }
            std::cout << instantiate_opts.descriptors;
            std::cout << "\n";
        }},
        {"env", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == help_argument) {
                    std::cout << "prints the current environment variables\n";
                    return;
                }
            }
            flow::pretty_print(std::cout, instantiate_opts.environment, "\n");
            std::cout.flush();
        }},
        {"setenv", [&](const string_span& args){
            auto usage = [&](std::ostream& os){
                os << "usage: ";
                os << args[0];
                os << " [--<flag>] [<env-name> <env-value>]\n";
                os << "  where <floag> may be:\n";
                os << "  --usage: shows this usage.\n";
                os << "  ";
                os << help_argument;
                os << ": shows help on this command.\n";
                os << "  --reset: resets the flow environment to given.\n";
            };
            auto argc = 0;
            for (auto&& arg: args) {
                if (arg == "--usage") {
                    usage(std::cout);
                    return;
                }
                if (arg == help_argument) {
                    std::cout << "sets the named environment variable ";
                    std::cout << "to the given value\n";
                    return;
                }
                if (arg == "--reset") {
                    instantiate_opts.environment = flow::get_environ();
                    --argc;
                    continue;
                }
            }
            switch (argc) {
            case 1:
                break;
            case 3:
                instantiate_opts.environment[args[1]] = (args.size() > 2)?
                    args[2]: "";
                break;
            default:
                usage(std::cerr);
                break;
            }
        }},
        {"unsetenv", [&](const string_span& args){
            for (auto&& arg: args.subspan(1u)) {
                if (arg == help_argument) {
                    std::cout << "unsets the named environment variables\n";
                    return;
                }
                if (arg == "--all") {
                    instantiate_opts.environment.clear();
                    continue;
                }
                if (instantiate_opts.environment.erase(arg) == 0) {
                    std::cerr << "no such environment variable as ";
                    std::cerr << std::quoted(arg);
                    std::cerr << "\n" << std::flush;
                }
            }
        }},
        {"show-systems", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == help_argument) {
                    std::cout << "shows a listing of system definitions.\n";
                    return;
                }
            }
            if (custom.subsystems.empty()) {
                std::cout << "empty.\n";
                return;
            }
            for (auto&& entry: custom.subsystems) {
                std::cout << entry.first << "=" << entry.second << "\n";
            }
        }},
        {"show-instances", [&](const string_span& args){
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
        }},
        {"add-executable", [&](const string_span& args){
            static const auto file_prefix = std::string{"--file="};
            auto system = flow::system{flow::system::executable{}};
            auto& info = std::get<flow::system::executable>(system.info);
            auto name = std::string{};
            auto index = 1u;
            auto usage = [&](std::ostream& os){
                os << "  usage: ";
                os << args[0];
                os << " ";
                os << help_argument;
                os << " | " << name_prefix << "<name> --file=<file>";
                os << " [--des-<n>=<in|out>[:<comment>]]";
                os << " arg...\n";
            };
            for (auto&& arg: args.subspan(1u)) {
                ++index;
                if (arg == help_argument) {
                    std::cout << "adds a new executable system definition.\n";
                    usage(std::cout);
                    return;
                }
                if (arg == "--usage") {
                    usage(std::cout);
                    return;
                }
                if (arg.starts_with(name_prefix)) {
                    name = arg.substr(name_prefix.length());
                    continue;
                }
                if (arg.starts_with(file_prefix)) {
                    info.file = {arg.substr(file_prefix.length())};
                    continue;
                }
                if (arg.starts_with(des_prefix)) {
                    if (const auto p = parse_descriptor_map_entry({
                        begin(arg) + size(des_prefix), end(arg)
                    })) {
                        if (p->second.direction == flow::io_type::none) {
                            system.descriptors.erase(p->first);
                        }
                        else if (const auto ret = system.descriptors.emplace(*p);
                            !ret.second) {
                            ret.first->second = p->second;
                        }
                    }
                    continue;
                }
                --index;
                break;
            }
            if (empty(name)) {
                std::cerr << "aborting: name must be specified\n";
                return;
            }
            const auto arg_span = args.subspan(index);
            info.arguments = std::vector(begin(arg_span), end(arg_span));
            if (!custom.subsystems.emplace(name, system).second) {
                std::cerr << "aborting: named system already exists\n";
            }
        }},
        {"add-custom", [&](const string_span& args){
            auto name = std::string{};
            auto parent = std::string{};
            auto usage = [&](std::ostream& os){
                os << "  usage: ";
                os << args[0];
                os << " ";
                os << help_argument;
                os << " | ";
                os << name_prefix << "<name>";
                os << " [" << parent_prefix << "<name>]";
                os << " [--des-<n>=<in|out>[:<comment>]]";
                os << " arg...\n";
            };
            for (auto&& arg: args.subspan(1u)) {
                if (arg == help_argument) {
                    std::cout << "adds a new custom system definition.\n";
                    usage(std::cout);
                    return;
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
            if (empty(name)) {
                std::cerr << "aborting: name must be specified\n";
                return;
            }
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
            const auto parent_basis = parse({
                {},
                flow::to_system_names(parent),
                &system
            });
            if (!empty(parent) && !empty(parent_basis.remaining)) {
                abort(std::cerr, name, parent, "no such parent");
                return;
            }
            const auto name_basis = parse({
                parent_basis.names,
                flow::to_system_names(name),
                parent_basis.psystem});
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
            if (!psys->subsystems.emplace(name_basis.remaining.front(),
                                          flow::system{flow::system::custom{}}).second) {
                abort(std::cerr, name, parent, "reason unknown");
                return;
            }
        }},
        {"wait", [&](const string_span& args){
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
        history(hist.get(), &ev, continuation? H_APPEND: H_ENTER, buf);
        continuation = tok_line_rv > 0;
        if (continuation) {
            continue;
        }
        if (ac < 1) {
            std::cerr << "unexpexred argument count of " << ac << "\n";
            continue;
        }
        if (const auto it = cmds.find(av[0]); it != cmds.end()) {
            arguments args;
            for (auto i = 0; i < ac; ++i) {
                args.emplace_back(av[i]);
            }
            it->second(args);
        }
        else if (const auto it = custom.subsystems.find(flow::system_name{av[0]});
                 it != custom.subsystems.end()) {
            const auto cmdname = flow::system_name{av[0]};
            auto tsys = it->second;
            if (const auto p = std::get_if<flow::system::executable>(&tsys.info)) {
                if (ac > 1) {
                    std::copy(av, av + ac, std::back_inserter(p->arguments));
                }
            }
            const auto name = std::string{av[0]} + ((ac > 1)? "+": "-") +
                std::to_string(sequence);
            try {
                auto obj = instantiate(tsys, std::cerr, instantiate_opts);
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
