#include <cstring> // for std::strcmp

#include <unistd.h> // for getpid
#include <fcntl.h> // for ::open

#include "system_error_code.hpp"

#include "flow/instance.hpp"
#include "flow/prototype.hpp"
#include "flow/utility.hpp"

namespace flow {

namespace {

constexpr auto pid_buffer_size = 32u;
using pid_buffer = std::array<char, pid_buffer_size>;

auto getpid_as_char_array() -> pid_buffer
{
    pid_buffer buffer{};
    std::snprintf( // NOLINT(cppcoreguidelines-pro-type-vararg)
                  data(buffer), size(buffer) - 1u, "%d", ::getpid());
    return buffer;
}

auto make_substitutions(std::vector<char*>& argv)
    -> void
{
    static constexpr const auto pid_request = "$$";
    for (auto&& arg: argv) {
        if (arg && std::strcmp(arg, pid_request) == 0) {
            static auto pid_argv = getpid_as_char_array();
            arg = std::data(pid_argv);
        }
    }
}

auto setup(const prototype_name& name,
           const pipe_connection& c,
           pipe_channel& p,
           std::ostream& errs) -> void
{
    if ((c.in.address != name) && (c.out.address != name)) {
        errs << name << " (unaffiliation) " << c;
        errs << " " << p << ", close in & out setup\n";
        const auto closed_in = p.close(io_type::in, errs);
        const auto closed_out = p.close(io_type::out, errs);
        if (!closed_in || !closed_out) {
            errs << ", for " << name << "\n";
            ::_exit(1);
        }
        return;
    }
    if (c.in.address == name) {
        errs << name << " " << c << " " << p;
        errs << ", close in, dup out to " << c.in.descriptor << " setup\n";
        if (!p.close(io_type::in, errs) || // close unused read end
            !p.dup(io_type::out, c.in.descriptor, errs)) {
            errs << ", for " << name << "\n";
            ::_exit(1);
        }
    }
    if (c.out.address == name) {
        errs << name << " " << c << " " << p;
        errs << ", close out, dup in to " << c.out.descriptor << " setup\n";
        if (!p.close(io_type::out, errs) || // close unused write end
            !p.dup(io_type::in, c.out.descriptor, errs)) {
            errs << ", for " << name << "\n";
            ::_exit(1);
        }
    }
}

auto setup(const prototype_name& name,
           const file_connection& c,
           std::ostream& errs) -> void
{
    errs << name << " " << c << ", open & dup2\n";
    const auto flags = to_open_flags(c.direction);
    const auto mode = 0600;
    const auto fd = ::open(c.file.path.c_str(), flags, mode); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (fd == -1) {
        static constexpr auto mode_width = 5;
        errs << "open file " << c.file.path << " with mode ";
        errs << std::oct << std::setfill('0') << std::setw(mode_width) << flags;
        errs << " failed: " << system_error_code(errno) << "\n";
        ::_exit(1);
    }
    if (::dup2(fd, int(c.process.descriptor)) == -1) {
        errs << "dup2(" << fd << "," << c.process.descriptor << ") failed: ";
        errs << system_error_code(errno) << "\n";
        ::_exit(1);
    }
}

[[noreturn]]
auto instantiate(const prototype_name& name,
                 const executable_prototype& exe_proto,
                 char * const argv[],
                 char * const envp[],
                 const std::vector<connection>& connections,
                 std::vector<channel>& channels,
                 std::ostream& errs) -> void
{
    // Deal with:
    // flow::pipe_connection{
    //   flow::process_port{ls_process_name, flow::descriptor_id{1}},
    //   flow::process_port{cat_process_name, flow::descriptor_id{0}}}
    //
    // Also:
    // Close file descriptors inherited by child that it's not using.
    // See: https://stackoverflow.com/a/7976880/7410358
    for (const auto& connection: connections) {
        const auto index = static_cast<std::size_t>(&connection - connections.data());
        if (const auto c = std::get_if<pipe_connection>(&connection)) {
            setup(name, *c, std::get<pipe_channel>(channels[index]), errs);
        }
        else if (const auto c = std::get_if<file_connection>(&connection)) {
            if (c->process.address == name) {
                setup(name, *c, errs);
            }
        }
    }
    if (!exe_proto.working_directory.empty()) {
        auto ec = std::error_code{};
        std::filesystem::current_path(exe_proto.working_directory, ec);
        if (ec) {
            errs << "chdir " << exe_proto.working_directory << " failed: ";
            write(errs, ec);
            errs << "\n";
        }
    }
    errs.flush();
    ::execve(exe_proto.executable_file.c_str(), argv, envp);
    const auto ec = system_error_code(errno);
    errs << "execve of " << exe_proto.executable_file << "failed: " << ec << "\n";
    errs.flush();
    ::_exit(1);
}

}

std::ostream& operator<<(std::ostream& os, const instance& value)
{
    os << "instance{";
    os << ".id=" << value.id;
    os << ",.children={";
    for (auto&& entry: value.children) {
        if (&entry != &(*value.children.begin())) {
            os << ",";
        }
        os << "{";
        os << ".first=" << entry.first;
        os << ",.second=" << entry.second;
        os << "}";
    }
    os << "}";
    os << ",.channels={";
    for (auto&& elem: value.channels) {
        if (&elem != &(*value.channels.begin())) {
            os << ",";
        }
        os << elem;
    }
    os << "}";
    os << "}";
    return os;
}

auto instantiate(const system_prototype& system, std::ostream& err_stream)
-> instance
{
    instance result;
    result.id = process_id(0);
    std::vector<channel> channels;
    for (auto&& connection: system.connections) {
        if (std::holds_alternative<pipe_connection>(connection)) {
            channels.push_back(pipe_channel{});
        }
        else if (std::holds_alternative<file_connection>(connection)) {
            channels.push_back(file_channel{}); // NOLINT(modernize-use-emplace)
        }
    }
    for (auto&& prototype_mapentry: system.prototypes) {
        const auto& name = prototype_mapentry.first;
        const auto& prototype = prototype_mapentry.second;
        if (std::holds_alternative<executable_prototype>(prototype)) {
            auto errs = temporary_fstream();
            const auto& exe_proto = std::get<executable_prototype>(prototype);
            auto arg_buffers = make_arg_bufs(exe_proto.arguments, exe_proto.executable_file);
            auto env_buffers = make_arg_bufs(exe_proto.environment);
            auto argv = make_argv(arg_buffers);
            auto envp = make_argv(env_buffers);
            const auto pid = process_id{::fork()};
            switch (pid) {
            case invalid_process_id:
                err_stream << "fork failed: " << system_error_code(errno) << "\n";
                result.children.emplace(name, instance{pid});
                break;
            case process_id{0}: { // child process
                // Have to be especially careful here!
                // From https://man7.org/linux/man-pages/man2/fork.2.html:
                // "child can safely call only async-signal-safe functions
                //  (see signal-safety(7)) until such time as it calls execve(2)."
                // See https://man7.org/linux/man-pages/man7/signal-safety.7.html
                // for "functions required to be async-signal-safe by POSIX.1".
                make_substitutions(argv);
                // NOTE: the following does not return!
                instantiate(name, exe_proto, argv.data(), envp.data(),
                            system.connections, channels, errs);
            }
            default: // case for the spawning/parent process
                result.children.emplace(name, instance{pid, std::move(errs)});
                break;
            }
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

}
