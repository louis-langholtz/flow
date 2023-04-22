#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <numeric> // for std::accumulate
#include <span>
#include <string>
#include <type_traits>
#include <utility> // for std::move
#include <variant>
#include <vector>

#include <fcntl.h> // for open
#include <sys/wait.h>
#include <unistd.h> // for fork, getcwd
#include <sys/types.h> // for mkfifo
#include <sys/stat.h> // for mkfifo

#include "flow/descriptor_id.hpp"
#include "flow/io_type.hpp"
#include "flow/pipe.hpp"
#include "flow/process_id.hpp"
#include "flow/process_name.hpp"

namespace flow {

using argument_container = std::vector<std::string>;
using environment_container = std::map<std::string, std::string>;

struct descriptor {
    std::string comment;
    io_type direction;
};

using descriptor_container = std::map<descriptor_id, descriptor>;

inline const auto standard_descriptors = descriptor_container{
    {descriptor_id{0}, {"stdin", io_type::in}},
    {descriptor_id{1}, {"stdout", io_type::out}},
    {descriptor_id{2}, {"stderr", io_type::out}},
};

struct process_instance {
    process_name prototype;
    process_id id;
};

struct process_port {
    process_name address;
    descriptor_id descriptor{invalid_descriptor_id}; ///< Well known descriptor ID for port.
};

struct file_port {
    std::string path;
};

using port = std::variant<process_port, file_port>;

/// @note Results in a <code>pipe</code>.
struct pipe_connection {
    process_port in;
    process_port out;
};

struct file_connection {
    file_port file;
    io_type direction;
    process_port process;
};

using connection = std::variant<pipe_connection, file_connection>;

struct file_channel {
};

using channel = std::variant<pipe, file_channel>;

struct system_instance {
    process_name prototype;
    std::map<process_name, process_id> process_ids;
    std::vector<channel> channels;
};

struct system_prototype;
struct executable_prototype;

using process_prototype = std::variant<executable_prototype, system_prototype>;

struct executable_prototype {
    descriptor_container descriptors;
    std::string working_directory;
    std::string path;
    argument_container arguments;
    environment_container environment;
};

struct system_prototype {
    descriptor_container descriptors;
    std::map<process_name, process_prototype> process_prototypes;
    std::vector<connection> connections;
};

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
std::vector<std::string>
make_arg_bufs(const std::vector<std::string>& strings,
              const std::string& fallback = {})
{
    auto result = std::vector<std::string>{};
    if (strings.empty()) {
        if (!fallback.empty()) {
            result.push_back(fallback);
        }
    }
    else {
        for (auto&& string: strings) {
            result.push_back(string);
        }
    }
    return result;
}

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
std::vector<std::string>
make_arg_bufs(const std::map<std::string, std::string>& envars)
{
    auto result = std::vector<std::string>{};
    for (const auto& envar: envars) {
        result.push_back(envar.first + "=" + envar.second);
    }
    return result;
}

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
std::vector<char*> make_argv(const std::span<std::string>& args)
{
    auto result = std::vector<char*>{};
    for (auto&& arg: args) {
        result.push_back(arg.data());
    }
    result.push_back(nullptr); // last element must always be nullptr!
    return result;
}

int to_open_flags(io_type direction)
{
    switch (direction) {
    case io_type::in: return O_RDONLY;
    case io_type::out: return O_WRONLY;
    }
    throw std::logic_error{"unknown direction"};
}

process_id instantiate(const process_name& name,
                       const executable_prototype& exe_proto,
                       const std::vector<connection>& connections,
                       std::vector<channel>& channels)
{
    auto arg_buffers = make_arg_bufs(exe_proto.arguments, exe_proto.path);
    auto argv = make_argv(arg_buffers);
    auto env_buffers = make_arg_bufs(exe_proto.environment);
    auto envp = make_argv(env_buffers);

    const auto pid = process_id{fork()};
    switch (pid) {
    case invalid_process_id:
        perror("fork");
        break;
    case process_id{0}: // child process
    {
        // Deal with:
        // flow::pipe_connection{
        //   flow::process_port{ls_process_name, flow::descriptor_id{1}},
        //   flow::process_port{cat_process_name, flow::descriptor_id{0}}}
        //
        // Have to be especially careful here though!
        // From https://man7.org/linux/man-pages/man2/fork.2.html:
        // "child can safely call only async-signal-safe functions
        //  (see signal-safety(7)) until such time as it calls execve(2)."
        // See https://man7.org/linux/man-pages/man7/signal-safety.7.html
        // for "functions required to be async-signal-safe by POSIX.1".
        for (const auto& connection: connections) {
            const auto index = static_cast<std::size_t>(&connection - connections.data());
            if (const auto c = std::get_if<pipe_connection>(&connection)) {
                if (c->in.address == name) {
                    auto& p = std::get<pipe>(channels[index]);
                    try {
                        p.close(io_type::in);
                        p.dup(io_type::out, c->in.descriptor);
                    }
                    catch (const std::runtime_error& error) {
                        std::cerr << error.what() << '\n';
                        _exit(1);
                    }
                }
                if (c->out.address == name) {
                    auto& p = std::get<pipe>(channels[index]);
                    try {
                        p.close(io_type::out);
                        p.dup(io_type::in, c->out.descriptor);
                    }
                    catch (const std::runtime_error& error) {
                        std::cerr << error.what() << '\n';
                        _exit(1);
                    }
                }
            }
            else if (const auto c = std::get_if<file_connection>(&connection)) {
                if (c->process.address == name) {
                    const auto fd = ::open(c->file.path.c_str(), to_open_flags(c->direction));
                    if (fd == -1) {
                        std::cerr << "open file " << c->file.path << " failed: " << std::strerror(errno) << '\n';
                        _exit(1);
                    }
                    if (::dup2(fd, int(c->process.descriptor)) == -1) {
                        std::cerr << "dup2(" << fd << "," << c->process.descriptor << ") failed: " << std::strerror(errno) << '\n';
                        _exit(1);
                    }
                }
            }
        }
        if (execve(exe_proto.path.c_str(), argv.data(), envp.data()) == -1) {
            throw std::runtime_error{std::strerror(errno)};
        }
        break;
    }
    default: // case for the spawning/parent process
        for (const auto& connection: connections) {
            if (const auto c = std::get_if<pipe_connection>(&connection)) {
                if (c->in.address == process_name{}) {
                    const auto index = static_cast<std::size_t>(&connection - connections.data());
                    std::get<pipe>(channels[index]).close(io_type::in);
                }
                if (c->out.address == process_name{}) {
                    const auto index = static_cast<std::size_t>(&connection - connections.data());
                    std::get<pipe>(channels[index]).close(io_type::out);
                }
            }
        }
        break;
    }
    return pid;
}

system_instance instantiate(const system_prototype& system,
                            process_name name = {})
{
    system_instance result;
    result.prototype = std::move(name);
    std::vector<channel> channels;
    for (auto&& connection: system.connections) {
        if (std::holds_alternative<pipe_connection>(connection)) {
            channels.push_back(pipe{});
        }
        else if (std::holds_alternative<file_connection>(connection)) {
            channels.push_back(file_channel{});
        }
    }
    for (auto&& process_prototype: system.process_prototypes) {
        const auto& name = process_prototype.first;
        const auto& prototype = process_prototype.second;
        if (std::holds_alternative<executable_prototype>(prototype)) {
            const auto& exe_proto = std::get<executable_prototype>(prototype);
            result.process_ids.emplace(name, instantiate(name, exe_proto, system.connections, channels));
        }
        else {
            result.process_ids.emplace(name, process_id(0));
        }
    }
    for (const auto& connection: system.connections) {
        if (const auto c = std::get_if<pipe_connection>(&connection)) {
            if ((c->in.address != process_name{}) && (c->out.address != process_name{})) {
                const auto index = static_cast<std::size_t>(&connection - system.connections.data());
                auto& p = std::get<pipe>(channels[index]);
                p.close(io_type::in);
                p.close(io_type::out);
            }
        }
    }
    result.channels = std::move(channels);
    return result;
}

enum class wait_diags {
    none, yes,
};

void wait(system_instance& instance, wait_diags diags = wait_diags::none)
{
    if (diags == wait_diags::yes) {
        std::cerr << "wait called for " << std::size(instance.process_ids) << " pids\n";
    }
    int status = 0;
    auto pid = flow::invalid_process_id;
    while (((pid = flow::process_id(::wait(&status))) != flow::invalid_process_id) || (errno != ECHILD)) {
        if (pid == flow::invalid_process_id) {
            const auto err = errno;
            if (err != EINTR) { // SIGCHLD is expected which causes EINTR
                std::cerr << "wait error: " << std::strerror(err) << '\n';
            }
            continue;
        }
        const auto it = std::find_if(std::begin(instance.process_ids),
                                     std::end(instance.process_ids),
                                     [pid](const auto& e) {
            return e.second == pid;
        });
        if (it != std::end(instance.process_ids)) {
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) != 0) {
                    std::cerr << it->first << " exit status=";
                    std::cerr << WEXITSTATUS(status);
                    std::cerr << '\n';
                }
                else {
                    if (diags == wait_diags::yes) {
                        std::cerr << "wait detected normal exit of " << it->first << '\n';
                    }
                }
                it->second = process_id(0);
            }
            else {
                std::cerr << "unexpected status " << status << " from process " << it->first << '\n';
            }
        }
        else {
            std::cerr << "unknown pid " << pid << '\n';
        }
    }
}

void touch(const file_port& file)
{
    if (const auto fd = ::creat(file.path.c_str(), 0666); fd != -1) {
        ::close(fd);
        return;
    }
    throw std::runtime_error{std::strerror(errno)};
}

void mkfifo(const file_port& file)
{
    if (::mkfifo(file.path.c_str(), 0666) == -1) {
        throw std::runtime_error{std::strerror(errno)};
    }
}

}

int main(int argc, const char * argv[])
{
    const auto input_file_port = flow::file_port{"flow.in"};

    const auto output_file_port = flow::file_port{"flow.out"};
    touch(output_file_port);
    std::cerr << "running in " << std::filesystem::current_path() << '\n';

    flow::system_prototype system;

    flow::executable_prototype cat_executable;
    cat_executable.path = "/bin/cat";
    const auto cat_process_name = flow::process_name{"cat"};
    system.process_prototypes.emplace(cat_process_name, cat_executable);

    flow::executable_prototype xargs_executable;
    xargs_executable.path = "/usr/bin/xargs";
    xargs_executable.arguments = {"xargs", "ls", "-alF"};
    const auto xargs_process_name = flow::process_name{"xargs"};
    system.process_prototypes.emplace(xargs_process_name, xargs_executable);

    system.connections.push_back(flow::file_connection{
        input_file_port,
        flow::io_type::in,
        flow::process_port{cat_process_name, flow::descriptor_id{0}},
    });
    system.connections.push_back(flow::pipe_connection{
        flow::process_port{cat_process_name, flow::descriptor_id{1}},
        flow::process_port{xargs_process_name, flow::descriptor_id{0}},
    });
    system.connections.push_back(flow::file_connection{
        output_file_port,
        flow::io_type::out,
        flow::process_port{xargs_process_name, flow::descriptor_id{1}}
    });

    auto instance = instantiate(system);
    wait(instance, flow::wait_diags::yes);

    std::cout << "system ran: ";
    std::cout << "name={" << instance.prototype << "}";
    std::cout << ", pids[" << std::size(instance.process_ids) << "]";
    std::cout << "\n";
    return 0;
}
