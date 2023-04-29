#include <cstring> // for std::strcmp

#include <unistd.h> // for getpid, setpgid
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
           std::ostream& diags) -> void
{
    if ((c.in.address != name) && (c.out.address != name)) {
        diags << name << " (unaffiliation) " << c;
        diags << " " << p << ", close in & out setup\n";
        const auto closed_in = p.close(io_type::in, diags);
        const auto closed_out = p.close(io_type::out, diags);
        if (!closed_in || !closed_out) {
            diags << ", for " << name << "\n";
            ::_exit(1);
        }
        return;
    }
    if (c.in.address == name) {
        diags << name << " " << c << " " << p;
        diags << ", close in, dup out to " << c.in.descriptor << " setup\n";
        if (!p.close(io_type::in, diags) || // close unused read end
            !p.dup(io_type::out, c.in.descriptor, diags)) {
            diags << ", for " << name << "\n";
            ::_exit(1);
        }
    }
    if (c.out.address == name) {
        diags << name << " " << c << " " << p;
        diags << ", close out, dup in to " << c.out.descriptor << " setup\n";
        if (!p.close(io_type::out, diags) || // close unused write end
            !p.dup(io_type::in, c.out.descriptor, diags)) {
            diags << ", for " << name << "\n";
            ::_exit(1);
        }
    }
}

auto setup(const prototype_name& name,
           const file_connection& c,
           std::ostream& diags) -> void
{
    diags << name << " " << c << ", open & dup2\n";
    const auto flags = to_open_flags(c.direction);
    const auto mode = 0600;
    const auto fd = ::open(c.file.path.c_str(), flags, mode); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (fd == -1) {
        static constexpr auto mode_width = 5;
        diags << "open file " << c.file.path << " with mode ";
        diags << std::oct << std::setfill('0') << std::setw(mode_width) << flags;
        diags << " failed: " << system_error_code(errno) << "\n";
        ::_exit(1);
    }
    if (::dup2(fd, int(c.process.descriptor)) == -1) {
        diags << "dup2(" << fd << "," << c.process.descriptor << ") failed: ";
        diags << system_error_code(errno) << "\n";
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
                 std::ostream& diags) -> void
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
            setup(name, *c, std::get<pipe_channel>(channels[index]), diags);
        }
        else if (const auto c = std::get_if<file_connection>(&connection)) {
            if (c->process.address == name) {
                setup(name, *c, diags);
            }
        }
    }
    if (!exe_proto.working_directory.empty()) {
        auto ec = std::error_code{};
        std::filesystem::current_path(exe_proto.working_directory, ec);
        if (ec) {
            diags << "chdir " << exe_proto.working_directory << " failed: ";
            write(diags, ec);
            diags << "\n";
        }
    }
    diags.flush();
    ::execve(exe_proto.executable_file.c_str(), argv, envp);
    const auto ec = system_error_code(errno);
    diags << "execve of " << exe_proto.executable_file << "failed: " << ec << "\n";
    diags.flush();
    ::_exit(1);
}

auto instantiate(const prototype_name& name,
                 const executable_prototype& exe_proto,
                 process_id pgrp,
                 const std::vector<connection>& connections,
                 std::vector<channel>& channels,
                 std::ostream& diags) -> instance
{
    auto child_diags = temporary_fstream();
    auto arg_buffers = make_arg_bufs(exe_proto.arguments, exe_proto.executable_file);
    auto env_buffers = make_arg_bufs(exe_proto.environment);
    auto argv = make_argv(arg_buffers);
    auto envp = make_argv(env_buffers);
    const auto pid = process_id{::fork()};
    switch (pid) {
    case invalid_process_id:
        diags << "fork failed: " << system_error_code(errno) << "\n";
        return instance{pid};
    case process_id{0}: { // child process
        // Have to be especially careful here!
        // From https://man7.org/linux/man-pages/man2/fork.2.html:
        // "child can safely call only async-signal-safe functions
        //  (see signal-safety(7)) until such time as it calls execve(2)."
        // See https://man7.org/linux/man-pages/man7/signal-safety.7.html
        // for "functions required to be async-signal-safe by POSIX.1".
        if (::setpgid(0, int(pgrp)) == -1) {
            child_diags << "setpgid failed(0, " << int(pgrp) << "): ";
            child_diags << system_error_code(errno);
            child_diags << "\n";
        }
        make_substitutions(argv);
        // NOTE: the following does not return!
        instantiate(name, exe_proto, argv.data(), envp.data(),
                    connections, channels, child_diags);
    }
    default: // case for the spawning/parent process
        diags << "child " << name << " started as pid " << pid << "\n";
        break;
    }
    return instance{pid, std::move(child_diags)};
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

auto instantiate(const system_prototype& system, std::ostream& diags)
-> instance
{
    instance result;
    result.id = no_process_id;
    std::vector<channel> channels;
    for (auto&& connection: system.connections) {
        if (std::holds_alternative<pipe_connection>(connection)) {
            channels.push_back(pipe_channel{});
        }
        else if (std::holds_alternative<file_connection>(connection)) {
            channels.push_back(file_channel{}); // NOLINT(modernize-use-emplace)
        }
    }
    for (auto&& mapentry: system.prototypes) {
        const auto& child = mapentry.first;
        const auto& proto = mapentry.second;
        if (const auto p = std::get_if<executable_prototype>(&proto)) {
            auto kid = instantiate(child, *p, process_id(-int(result.id)),
                                   system.connections, channels, diags);
            if (kid.id != invalid_process_id && result.id == no_process_id) {
                result.id = process_id(-int(kid.id));
            }
            result.children.emplace(child, std::move(kid));
        }
        else if (const auto p = std::get_if<system_prototype>(&proto)) {
            result.children.emplace(child, instantiate(*p, diags));
        }
        else {
            diags << "parent: unrecognized prototype for " << child << "\n";
            result.children.emplace(child, instance{});
        }
    }
    for (const auto& connection: system.connections) {
        const auto index = static_cast<std::size_t>(&connection - &(*system.connections.begin()));
        if (const auto c = std::get_if<pipe_connection>(&connection)) {
            auto& p = std::get<pipe_channel>(channels[index]);
            if (c->in.address != prototype_name{}) {
                diags << "parent: close out of " << *c << " " << p << "\n";
                p.close(io_type::out, diags);
            }
            if (c->out.address != prototype_name{}) {
                diags << "parent: close  in of " << *c << " " << p << "\n";
                p.close(io_type::in, diags);
            }
        }
    }
    result.channels = std::move(channels);
    return result;
}

}
