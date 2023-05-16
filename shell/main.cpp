#include <functional> // for std::function
#include <iomanip> // for std::quoted
#include <iostream>
#include <iterator>
#include <ostream> // for std::flush
#include <span>
#include <string> // for std::getline

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/environment_map.hpp"
#include "flow/indenting_ostreambuf.hpp"
#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/system_name.hpp"
#include "flow/utility.hpp"

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

auto main(int argc, const char * argv[]) -> int
{
    using arguments = std::vector<std::string>;
    using string_span = std::span<const std::string>;
    using cmd_handler = std::function<void(const string_span& args)>;
    using cmd_table = std::map<std::string, cmd_handler>;

    flow::environment_map environment = flow::get_environ();
    std::map<flow::system_name, flow::system> systems;
    std::map<flow::system_name, flow::instance> instances;
    flow::set_signal_handler(flow::signal::interrupt);
    flow::set_signal_handler(flow::signal::terminate);

    // TODO: make this into a table of CRUD commands...
    const cmd_table cmds{
        {"exit", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == "--help") {
                    std::cout << "exits the shell\n";
                    return;
                }
            }
            std::exit(0);
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
                (cmds.at(entry.first))(std::vector<std::string>{"--help"});
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
                os << " <env-name> <env-value>\n";
            };
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
            }
            if (args.size() < 2u) {
                usage(std::cerr);
                return;
            }
            environment[args[1]] = (args.size() > 2)? args[2]: "";
        }},
        {"unset-env", [&](const string_span& args){
            for (auto&& arg: args) {
                if (arg == "--help") {
                    std::cout << "unsets the named environment variable\n";
                    return;
                }
            }
            if (args.size() < 2u) {
                std::cerr << "usage: ";
                std::cerr << args[0];
                std::cerr << " <env-name>\n";
                return;
            }
            environment.erase(args[1]);
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
        {"add-system", [&](const string_span& args){
            auto system = flow::system{};
            auto name = flow::system_name{};
            for (auto&& arg: args) {
                if (arg == "--help") {
                    std::cout << "adds a new system definition.\n";
                    return;
                }
                if (arg == "custom") {
                    system.info = flow::system::custom{};
                    continue;
                }
                if (arg == "executable") {
                    system.info = flow::system::executable{};
                    continue;
                }
                static const auto name_prefix = std::string{"name="};
                if (arg.starts_with(name_prefix)) {
                    name = {arg.substr(name_prefix.length())};
                    continue;
                }
            }
            systems.emplace(name, system);
        }},
    };

    auto prompt = std::string{"> "};
    std::cout << prompt << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
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
        std::cout << prompt << std::flush;
    }

    return 0;
}
