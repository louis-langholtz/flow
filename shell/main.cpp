#include <array>
#include <iostream>
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
#include <unistd.h> // for fork, getcwd
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
                    errs << name << " (unaffiliation) " << *c << " " << p << ", close in&out setup\n";
                    const auto closed_in = p.close(io_type::in, errs);
                    const auto closed_out = p.close(io_type::out, errs);
                    if (!closed_in || !closed_out) {
                        errs << ", for " << name << "\n";
                        _exit(1);
                    }
                    continue;
                }
                if (c->in.address == name) {
                    errs << name << " " << *c << " " << p << ", close in, dup out to " << c->in.descriptor << " setup\n";
                    if (!p.close(io_type::in, errs) || // close unused read end
                        !p.dup(io_type::out, c->in.descriptor, errs)) {
                        errs << ", for " << name << "\n";
                        _exit(1);
                    }
                }
                if (c->out.address == name) {
                    errs << name << " " << *c << " " << p << ", close out, dup in to " << c->out.descriptor << " setup\n";
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
        if (execve(exe_proto.path.c_str(), argv.data(), envp.data()) == -1) {
            throw std::runtime_error{std::strerror(errno)};
        }
        break;
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
            const auto& exe_proto = std::get<executable_prototype>(prototype);
            const auto pid = instantiate(name, exe_proto, system.connections,
                                         channels, err_stream);
            result.children.emplace(name, instance{pid});
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

std::optional<std::size_t> find_index(const std::span<connection>& connections,
                                      const prototype_name& in, const prototype_name& out)
{
    for (auto&& connection: connections) {
        const auto index = static_cast<std::size_t>(&connection - &(*connections.begin()));
        if (const auto c = std::get_if<pipe_connection>(&connection)) {
            if (c->in.address == in && c->out.address == out) {
                return index;
            }
        }
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

    system.connections.push_back(flow::pipe_connection{
        flow::prototype_port{},
        flow::prototype_port{cat_process_name, flow::descriptor_id{0}},
    });
    system.connections.push_back(flow::pipe_connection{
        flow::prototype_port{cat_process_name, flow::descriptor_id{1}},
        flow::prototype_port{xargs_process_name, flow::descriptor_id{0}},
    });
#if 0
    system.connections.push_back(flow::file_connection{
        output_file_port,
        flow::io_type::out,
        flow::prototype_port{cat_process_name, flow::descriptor_id{1}}
    });
#endif

    auto instance = instantiate(system, std::cerr);
    if (const auto found = find_index(system.connections,
                                      flow::prototype_name{}, cat_process_name)) {
        std::cerr << "found channel\n";
        auto& channel = std::get<flow::pipe_channel>(instance.channels[*found]);
        std::cerr << "writing to channel: " << channel << "\n";
        channel.write("/bin\n/sbin\n", std::cerr);
        channel.close(flow::io_type::out, std::cerr);
    }
    wait(instance, std::cerr, flow::wait_diags::yes);

    std::cerr << "system ran: ";
    std::cerr << ", children[" << std::size(instance.children) << "]";
    std::cerr << "\n";
    return 0;
}
