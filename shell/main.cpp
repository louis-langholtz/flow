#include <cstdlib> // for std::strtol
#include <functional> // for std::function
#include <iomanip> // for std::quoted
#include <iostream>
#include <iterator>
#include <memory> // for std::unique_ptr
#include <ostream> // for std::flush
#include <span>
#include <string> // for std::getline

#include <histedit.h>

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/environment_map.hpp"
#include "flow/indenting_ostreambuf.hpp"
#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/system_name.hpp"
#include "flow/utility.hpp"

namespace {

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

char *prompt([[maybe_unused]] EditLine *el)
{
    static char buf[] = "\1\033[7m\1flow$\1\033[0m\1 ";
    return buf;
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

    flow::environment_map environment = flow::get_environ();
    std::map<flow::system_name, flow::system> systems;
    std::map<flow::system_name, flow::instance> instances;
    flow::set_signal_handler(flow::signal::interrupt);
    flow::set_signal_handler(flow::signal::terminate);
    auto do_loop = true;

    // For example of using libedit, see: https://tinyurl.com/3ez9utzc
    HistEvent ev{};
    auto hist = history_ptr{history_init()};
    history(hist.get(), &ev, H_SETSIZE, 100);

    auto el = edit_line_ptr{el_init(argv[0], stdin, stdout, stderr)};
    // el_parse(el.get(), argc, argv);

    el_set(el.get(), EL_SIGNAL, 1); // installs sig handlers for resizing, etc.
    el_set(el.get(), EL_HIST, history, hist.get());
    el_set(el.get(), EL_PROMPT_ESC, prompt, '\1');

    el_source(el.get(), NULL);

    // TODO: make this into a table of CRUD commands...
    const cmd_table cmds{
        {"exit", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == "--help") {
                    std::cout << "exits the shell\n";
                    return;
                }
            }
            do_loop = false;
        }},
        {"help", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == "--help") {
                    std::cout << "provides help on the available commands\n";
                    return;
                }
            }
            std::cout << "Available commands:\n";
            for (auto&& entry: cmds) {
                std::cout << entry.first << ": ";
                using strings = std::vector<std::string>;
                const auto cargs = strings{entry.first, "--help"};
                (cmds.at(entry.first))(cargs);
            }
        }},
        {"history", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == "--help") {
                    std::cout << "shows the history of commands entered\n";
                    return;
                }
                if (arg == "clear") {
                    history(hist.get(), &ev, H_CLEAR);
                    return;
                }
            }
            for (auto rv = history(hist.get(), &ev, H_LAST);
                 rv != -1;
                 rv = history(hist.get(), &ev, H_PREV)) {
                std::cout << ev.num << " " << ev.str;
            }
        }},
        {"show-env", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == "--help") {
                    std::cout << "prints the current environment variables\n";
                    return;
                }
            }
            flow::pretty_print(std::cout, environment, "\n");
            std::cout.flush();
        }},
        {"set-env", [&](const string_span& args){
            auto usage = [&](std::ostream& os){
                os << "usage: ";
                os << args[0];
                os << " [--<flag>] [<env-name> <env-value>]\n";
                os << "  where <floag> may be:\n";
                os << "  --usage: shows this usage.\n";
                os << "  --help: shows help on this command.\n";
                os << "  --reset: resets the flow environment to given.\n";
            };
            auto argc = 0;
            for (auto&& arg: args) {
                if (arg == "--usage") {
                    usage(std::cout);
                    return;
                }
                if (arg == "--help") {
                    std::cout << "sets the named environment variable ";
                    std::cout << "to the given value\n";
                    return;
                }
                if (arg == "--reset") {
                    environment = flow::get_environ();
                    --argc;
                    continue;
                }
            }
            switch (argc) {
            case 1:
                break;
            case 3:
                environment[args[1]] = (args.size() > 2)? args[2]: "";
                break;
            default:
                usage(std::cerr);
                break;
            }
        }},
        {"unset-env", [&](const string_span& args){
            for (auto&& arg: args.subspan(1u)) {
                if (arg == "--help") {
                    std::cout << "unsets the named environment variables\n";
                    return;
                }
                if (arg == "--all") {
                    environment.clear();
                    continue;
                }
                if (environment.erase(arg) == 0) {
                    std::cerr << "no such environment variable as ";
                    std::cerr << std::quoted(arg);
                    std::cerr << "\n" << std::flush;
                }
            }
        }},
        {"show-systems", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == "--help") {
                    std::cout << "shows a listing of system definitions.\n";
                    return;
                }
            }
            if (args.size() < 2u) {
                if (systems.empty()) {
                    std::cout << "empty.\n";
                    return;
                }
                for (auto&& entry: systems) {
                    std::cout << entry.first << "=" << entry.second << "\n";
                }
            }
        }},
        {"add-executable", [&](const string_span& args){
            static const auto name_prefix = std::string{"--name="};
            static const auto file_prefix = std::string{"--file="};
            static const auto des_prefix = std::string{"--des-"};
            static const auto in_str = std::string{"in"};
            static const auto out_str = std::string{"out"};
            auto system = flow::system{};
            system.info = flow::system::executable{};
            auto& info = std::get<flow::system::executable>(system.info);
            auto name = flow::system_name{};
            auto index = 1u;
            for (auto&& arg: args.subspan(1u)) {
                ++index;
                if (arg == "--help") {
                    std::cout << "adds a new executable system definition.\n";
                    std::cout << "  usage: ";
                    std::cout << args[0];
                    std::cout << " --name=<name> --file=<file>";
                    std::cout << " [--des-<n>=<in|out>[:<comment>]]";
                    std::cout << " arg...\n";
                    return;
                }
                if (arg.starts_with(name_prefix)) {
                    name = {arg.substr(name_prefix.length())};
                    continue;
                }
                if (arg.starts_with(file_prefix)) {
                    info.file = {arg.substr(file_prefix.length())};
                    continue;
                }
                if (arg.starts_with(des_prefix)) {
                    auto des_id = flow::descriptor_id{};
                    auto des_info = flow::descriptor_info{};
                    const auto rem = arg.substr(des_prefix.size());
                    char* p_end{};
                    const auto n = std::strtol(rem.c_str(), &p_end, 10);
                    if ((p_end == rem.c_str()) || (*p_end != '=')) {
                        std::cerr << "aborting: unrecognized argument ";
                        std::cerr << std::quoted(arg);
                        std::cerr << "\n";
                        return;
                    }
                    des_id = flow::descriptor_id(n);
                    ++p_end; // skip over '='
                    if (in_str.compare(0, in_str.size(),
                                       p_end, in_str.size()) == 0) {
                        des_info.direction = flow::io_type::in;
                        p_end += in_str.size();
                    }
                    else if (out_str.compare(0, out_str.size(),
                                             p_end, out_str.size()) == 0) {
                        des_info.direction = flow::io_type::out;
                        p_end += out_str.size();
                    }
                    else {
                        std::cerr << "aborting: unrecognized direction in ";
                        std::cerr << std::quoted(arg);
                        std::cerr << "\n";
                        return;
                    }
                    if ((*p_end != '\0') && (*p_end != ':')) {
                        std::cerr << "aborting: unrecognized ending of ";
                        std::cerr << std::quoted(arg);
                        std::cerr << " seeing '";
                        std::cerr << *p_end;
                        std::cerr << "'\n";
                        return;
                    }
                    if (*p_end == ':') {
                        ++p_end;
                        des_info.comment = std::string{p_end};
                    }
                    system.descriptors[des_id] = des_info;
                    continue;
                }
                --index;
                break;
            }
            const auto arg_span = args.subspan(index);
            info.arguments = std::vector(begin(arg_span), end(arg_span));
            systems.emplace(name, system);
        }},
    };

    auto count = 0;
    auto buf = static_cast<const char*>(nullptr);
    while (do_loop && ((buf = el_gets(el.get(), &count)) != nullptr)) {
        history(hist.get(), &ev, H_ENTER, buf);
        auto line = std::string{buf, buf + count};
        std::string command;
        arguments args;
        std::string word;
        std::istringstream in{line};
        while (in >> std::quoted(word)) {
            if (args.empty()) {
                command = word;
            }
            args.emplace_back(word);
        }
        if (const auto it = cmds.find(command); it != cmds.end()) {
            it->second(args);
        }
        else {
            std::cerr << "no such command as " << std::quoted(command) << ".\n";
            std::cerr << "enter " << std::quoted("help") << " for help.\n";
        }
    }
    return 0;
}
