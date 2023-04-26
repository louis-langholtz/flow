#include <algorithm> // for std::find
#include <array>
#include <iostream>
#include <iterator> // for std::distance
#include <map>
#include <memory>
#include <numeric> // for std::accumulate
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility> // for std::move
#include <vector>

#include <fcntl.h> // for open
#include <sys/wait.h>
#include <unistd.h> // for fork
#include <sys/types.h> // for mkfifo
#include <sys/stat.h> // for mkfifo

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/file_port.hpp"
#include "flow/instance.hpp"
#include "flow/io_type.hpp"
#include "flow/process_id.hpp"
#include "flow/prototype_name.hpp"
#include "flow/prototype_port.hpp"
#include "flow/prototype.hpp"
#include "flow/utility.hpp"

namespace flow {

process_id instantiate(const prototype_name& name,
                       const executable_prototype& exe_proto,
                       const std::vector<connection>& connections,
                       std::vector<channel>& channels,
                       std::ostream& errs)
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
        // Also:
        // Close file descriptors inherited by child that it's not using.
        // See: https://stackoverflow.com/a/7976880/7410358
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
                auto& p = std::get<pipe_channel>(channels[index]);
                if ((c->in.address != name) && (c->out.address != name)) {
                    errs << name << " (unaffiliation) " << *c;
                    errs << " " << p << ", close in & out setup\n";
                    const auto closed_in = p.close(io_type::in, errs);
                    const auto closed_out = p.close(io_type::out, errs);
                    if (!closed_in || !closed_out) {
                        errs << ", for " << name << "\n";
                        _exit(1);
                    }
                    continue;
                }
                if (c->in.address == name) {
                    errs << name << " " << *c << " " << p;
                    errs << ", close in, dup out to " << c->in.descriptor << " setup\n";
                    if (!p.close(io_type::in, errs) || // close unused read end
                        !p.dup(io_type::out, c->in.descriptor, errs)) {
                        errs << ", for " << name << "\n";
                        _exit(1);
                    }
                }
                if (c->out.address == name) {
                    errs << name << " " << *c << " " << p;
                    errs << ", close out, dup in to " << c->out.descriptor << " setup\n";
                    if (!p.close(io_type::out, errs) || // close unused write end
                        !p.dup(io_type::in, c->out.descriptor, errs)) {
                        errs << ", for " << name << "\n";
                        _exit(1);
                    }
                }
            }
            else if (const auto c = std::get_if<file_connection>(&connection)) {
                if (c->process.address == name) {
                    errs << name << " " << *c << ", open & dup2\n";
                    const auto fd = ::open(c->file.path.c_str(), to_open_flags(c->direction));
                    if (fd == -1) {
                        errs << "open file " << c->file.path << " failed: ";
                        errs << std::strerror(errno) << "\n";
                        _exit(1);
                    }
                    if (::dup2(fd, int(c->process.descriptor)) == -1) {
                        errs << "dup2(" << fd << "," << c->process.descriptor << ") failed: ";
                        errs << std::strerror(errno) << "\n";
                        _exit(1);
                    }
                }
            }
        }
        errs.flush();
        execve(exe_proto.path.c_str(), argv.data(), envp.data());
        errs << "execve failed: " << std::strerror(errno) << "\n";
        _exit(1);
    }
    default: // case for the spawning/parent process
        break;
    }
    return pid;
}

instance instantiate(const system_prototype& system, std::ostream& err_stream)
{
    instance result;
    result.id = process_id(0);
    std::vector<channel> channels;
    for (auto&& connection: system.connections) {
        if (std::holds_alternative<pipe_connection>(connection)) {
            channels.push_back(pipe_channel{});
        }
        else if (std::holds_alternative<file_connection>(connection)) {
            channels.push_back(file_channel{});
        }
    }
    for (auto&& prototype_mapentry: system.prototypes) {
        const auto& name = prototype_mapentry.first;
        const auto& prototype = prototype_mapentry.second;
        if (std::holds_alternative<executable_prototype>(prototype)) {
            auto errs = temporary_fstream();
            const auto& exe_proto = std::get<executable_prototype>(prototype);
            const auto pid = instantiate(name, exe_proto, system.connections,
                                         channels, errs);
            result.children.emplace(name, instance{pid, std::move(errs)});
        }
        else if (std::holds_alternative<system_prototype>(prototype)) {
            const auto& sys_proto = std::get<system_prototype>(prototype);
            result.children.emplace(name, instantiate(sys_proto, err_stream));
        }
    }
    for (const auto& connection: system.connections) {
        const auto index = static_cast<std::size_t>(&connection - &(*system.connections.begin()));
        if (const auto c = std::get_if<pipe_connection>(&connection)) {
            auto& p = std::get<pipe_channel>(channels[index]);
            if (c->in.address != prototype_name{}) {
                err_stream << "parent: close out of " << *c << " " << p << "\n";
                p.close(io_type::out, err_stream);
            }
            if (c->out.address != prototype_name{}) {
                err_stream << "parent: close  in of " << *c << " " << p << "\n";
                p.close(io_type::in, err_stream);
            }
        }
    }
    result.channels = std::move(channels);
    return result;
}

enum class wait_diags {
    none, yes,
};

void wait(instance& instance, std::ostream& err_stream,
          wait_diags diags = wait_diags::none)
{
    if (diags == wait_diags::yes) {
        err_stream << "wait called for " << std::size(instance.children) << " children\n";
    }
    int status = 0;
    auto pid = flow::invalid_process_id;
    while (((pid = flow::process_id(::wait(&status))) != flow::invalid_process_id) || (errno != ECHILD)) {
        if (pid == flow::invalid_process_id) {
            const auto err = errno;
            if (err != EINTR) { // SIGCHLD is expected which causes EINTR
                err_stream << "wait error: " << std::strerror(err) << '\n';
            }
            continue;
        }
        const auto it = std::find_if(std::begin(instance.children),
                                     std::end(instance.children),
                                     [pid](const auto& e) {
            return e.second.id == pid;
        });
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                if (it != std::end(instance.children)) {
                    err_stream << "child=" << it->first << ", ";
                }
                err_stream << "pid=" << pid;
                err_stream << ", exit status=" << WEXITSTATUS(status);
                err_stream << "\n";
            }
            else {
                if (diags == wait_diags::yes) {
                    err_stream << "child=";
                    if (it != std::end(instance.children)) {
                        err_stream << it->first;
                    }
                    else {
                        err_stream << "unknown";
                    }
                    err_stream << ", pid=" << pid;
                    err_stream << ", normal exit\n";
                }
            }
            if (it != std::end(instance.children)) {
                it->second.id = process_id(0);
            }
        }
        else if (WIFSIGNALED(status)) {
            err_stream << "child signaled " << WTERMSIG(status) << "\n";
        }
        else {
            err_stream << "unexpected status " << status << " from process " << it->first << '\n';
        }
    }
}

auto find_index(const std::span<connection>& connections,
                const connection& look_for) -> std::optional<std::size_t>
{
    const auto first = std::begin(connections);
    const auto last = std::end(connections);
    const auto iter = std::find(first, last, look_for);
    if (iter != last) {
        return {std::distance(first, iter)};
    }
    return {};
}

void touch(const file_port& file)
{
    if (const auto fd = ::open(file.path.c_str(), O_CREAT|O_WRONLY, 0666); fd != -1) {
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
    const auto cat_process_name = flow::prototype_name{"cat"};
    system.prototypes.emplace(cat_process_name, cat_executable);

    flow::executable_prototype xargs_executable;
    xargs_executable.path = "/usr/bin/xargs";
    xargs_executable.arguments = {"xargs", "ls", "-alF"};
    const auto xargs_process_name = flow::prototype_name{"xargs"};
    system.prototypes.emplace(xargs_process_name, xargs_executable);

    const auto cat_stdin = flow::pipe_connection{
        flow::prototype_port{},
        flow::prototype_port{cat_process_name, flow::descriptor_id{0}}
    };
    system.connections.push_back(cat_stdin);
    system.connections.push_back(flow::pipe_connection{
        flow::prototype_port{cat_process_name, flow::descriptor_id{1}},
        flow::prototype_port{xargs_process_name, flow::descriptor_id{0}},
    });
    const auto xargs_stdout = flow::pipe_connection{
        flow::prototype_port{xargs_process_name, flow::descriptor_id{1}},
        flow::prototype_port{},
    };
    system.connections.push_back(xargs_stdout);
    const auto xargs_stderr = flow::pipe_connection{
        flow::prototype_port{xargs_process_name, flow::descriptor_id{2}},
        flow::prototype_port{},
    };
    system.connections.push_back(xargs_stderr);
#if 0
    system.connections.push_back(flow::file_connection{
        output_file_port,
        flow::io_type::out,
        flow::prototype_port{cat_process_name, flow::descriptor_id{1}}
    });
#endif

    {
        auto errs = flow::temporary_fstream();
        auto instance = instantiate(system, errs);
        if (const auto found = find_index(system.connections, cat_stdin)) {
            auto& channel = std::get<flow::pipe_channel>(instance.channels[*found]);
            channel.write("/bin\n/sbin", std::cerr);
            channel.close(flow::io_type::out, std::cerr);
        }
        wait(instance, std::cerr, flow::wait_diags::none);
        if (const auto found = find_index(system.connections, xargs_stderr)) {
            auto& channel = std::get<flow::pipe_channel>(instance.channels[*found]);
            std::array<char, 1024> buffer{};
            const auto nread = channel.read(buffer, std::cerr);
            if (nread == static_cast<std::size_t>(-1)) {
                std::cerr << "xargs can't read stderr\n";
            }
            else if (nread != 0u) {
                std::cerr << "xargs stderr: " << buffer.data() << "\n";
            }
        }
        if (const auto found = find_index(system.connections, xargs_stdout)) {
            auto& channel = std::get<flow::pipe_channel>(instance.channels[*found]);
            for (;;) {
                std::array<char, 4096> buffer{};
                const auto nread = channel.read(buffer, std::cerr);
                if (nread == static_cast<std::size_t>(-1)) {
                    std::cerr << "xargs can't read stdout\n";
                }
                else if (nread != 0u) {
                    std::cout << buffer.data();
                }
                else {
                    break;
                }
            }
        }
        std::cerr << "Diagnostics for root instance...\n";
        errs.seekg(0);
        std::copy(std::istreambuf_iterator<char>(errs),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));

        show_diags(flow::prototype_name{}, instance, std::cerr);
        std::cerr << "system ran: ";
        std::cerr << ", children[" << std::size(instance.children) << "]";
        std::cerr << "\n";
    }
    return 0;
}
