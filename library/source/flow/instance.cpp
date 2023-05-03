#include <cstring> // for std::strcmp

#include <unistd.h> // for getpid, setpgid
#include <fcntl.h> // for ::open

#include "indenting_ostreambuf.hpp"
#include "os_error_code.hpp"

#include "flow/instance.hpp"
#include "flow/system.hpp"
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

template <class T>
auto make_endpoints(const unidirectional_connection& c)
    -> std::array<const T*, 2u>
{
    return {std::get_if<T>(&c.src), std::get_if<T>(&c.dst)};
}

template <class T>
auto make_endpoints(const bidirectional_connection& c)
    -> std::array<const T*, 2u>
{
    return {
        std::get_if<T>(&c.ends[0]), // NOLINT(readability-container-data-pointer)
        std::get_if<T>(&c.ends[1])
    };
}

template <class T>
auto make_endpoints(const connection& c) -> std::array<const T*, 2u>
{
    if (const auto p = std::get_if<unidirectional_connection>(&c)) {
        return make_endpoints<T>(*p);
    }
    if (const auto p = std::get_if<bidirectional_connection>(&c)) {
        return make_endpoints<T>(*p);
    }
    return std::array<const T*, 2u>{nullptr, nullptr};
}

auto setup(const system_name& name,
           const connection& c,
           pipe_channel& p,
           std::ostream& diags) -> void
{
    const auto ends = make_endpoints<system_endpoint>(c);
    if (!ends[0] && !ends[1]) {
        diags << "connection has no system_endpoint: " << c << "\n";
        return;
    }
    if ((!ends[0] || (ends[0]->address != name)) &&
        (!ends[1] || (ends[1]->address != name))) {
        diags << name << " (unaffiliation) " << c;
        diags << " " << p << ", close in & out setup\n";
        const auto closed_in = p.close(pipe_channel::io::read, diags);
        const auto closed_out = p.close(pipe_channel::io::write, diags);
        if (!closed_in || !closed_out) {
            diags << ", for " << name << "\n";
            ::_exit(1);
        }
        return;
    }
    if (ends[0]) { // src
        if (ends[0]->address == name) {
            diags << name << " " << c << " " << p;
            diags << ", close  read-side\n";
            if (!p.close(pipe_channel::io::read, diags)) { // close unused end
                ::_exit(1);
            }
            diags << name << " " << c << " " << p;
            diags << ", dup write-side to " << ends[0]->descriptor << "\n";
            if (!p.dup(pipe_channel::io::write, ends[0]->descriptor, diags)) {
                ::_exit(1);
            }
        }
        else {
            diags << name << " " << c << " " << p;
            diags << ", close write-side\n";
#if 0
            if (!p.close(pipe_channel::io::write, diags)) { // close unused end
                ::_exit(1);
            }
#endif
        }
    }
    if (ends[1]) { // dst
        if (ends[1]->address == name) {
            diags << name << " " << c << " " << p;
            diags << ", close write-side\n";
            if (!p.close(pipe_channel::io::write, diags)) { // close unused end
                ::_exit(1);
            }
            diags << name << " " << c << " " << p;
            diags << ", dup  read-side to " << ends[1]->descriptor << "\n";
            if (!p.dup(pipe_channel::io::read, ends[1]->descriptor, diags)) {
                ::_exit(1);
            }
        }
        else {
            diags << name << " " << c << " " << p;
            diags << ", close  read-side\n";
#if 0
            if (!p.close(pipe_channel::io::read, diags)) { // close unused end
                ::_exit(1);
            }
#endif
        }
    }
}

auto to_open_flags(io_type direction) noexcept -> int
{
    switch (direction) {
    case io_type::in: return O_RDONLY;
    case io_type::out: return O_WRONLY;
    case io_type::bidir: return O_RDWR;
    }
    return 0;
}

auto setup(const system_name& name,
           const bidirectional_connection& c,
           file_channel& p,
           std::ostream& diags) -> void
{
    const auto file_ends = make_endpoints<file_endpoint>(c);
    const auto& file_end = file_ends[0]? file_ends[0]: file_ends[1];
    const auto& other_end = file_ends[0]? c.ends[1]: c.ends[0];
    if (const auto op = std::get_if<system_endpoint>(&other_end)) {
        if (op->address != name) {
            diags << name << " skipping " << c << "\n";
            return;
        }
        const auto flags = to_open_flags(p.io);
        const auto mode = 0600;
        const auto fd = ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
                               file_end->path.c_str(), flags, mode);
        if (fd == -1) {
            static constexpr auto mode_width = 5;
            diags << name << " " << c;
            diags << ", open file " << file_end->path << " with mode ";
            diags << std::oct << std::setfill('0') << std::setw(mode_width) << flags;
            diags << " failed: " << os_error_code(errno) << "\n";
            ::_exit(1);
        }
        if (::dup2(fd, int(op->descriptor)) == -1) {
            diags << name << " " << c;
            diags << ", dup2(" << fd << "," << op->descriptor << ") failed: ";
            diags << os_error_code(errno) << "\n";
            ::_exit(1);
        }
    }
}

auto setup(const system_name& name,
           const unidirectional_connection& c,
           file_channel& p,
           std::ostream& diags) -> void
{
    const auto file_ends = make_endpoints<file_endpoint>(c);
    const auto& file_end = file_ends[0]? file_ends[0]: file_ends[1];
    const auto& other_end = file_ends[0]? c.dst: c.src;
    if (const auto op = std::get_if<system_endpoint>(&other_end)) {
        if (op->address != name) {
            diags << name << " skipping " << c << "\n";
            return;
        }
        const auto flags = to_open_flags(p.io);
        const auto mode = 0600;
        const auto fd = ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
                               file_end->path.c_str(), flags, mode);
        if (fd == -1) {
            static constexpr auto mode_width = 5;
            diags << name << " " << c;
            diags << ", open file " << file_end->path << " with mode ";
            diags << std::oct << std::setfill('0') << std::setw(mode_width) << flags;
            diags << " failed: " << os_error_code(errno) << "\n";
            ::_exit(1);
        }
        if (::dup2(fd, int(op->descriptor)) == -1) {
            diags << name << " " << c;
            diags << ", dup2(" << fd << "," << op->descriptor << ") failed: ";
            diags << os_error_code(errno) << "\n";
            ::_exit(1);
        }
    }
}

auto setup(const system_name& name,
           const connection& conn,
           file_channel& fchan,
           std::ostream& diags) -> void
{
    if (const auto p = std::get_if<unidirectional_connection>(&conn)) {
        return setup(name, *p, fchan, diags);
    }
    if (const auto p = std::get_if<bidirectional_connection>(&conn)) {
        return setup(name, *p, fchan, diags);
    }
}

auto setup(const system_name& name,
           const connection& c,
           channel& channel,
           std::ostream& diags) -> void
{
    if (const auto p = std::get_if<pipe_channel>(&channel)) {
        setup(name, c, *p, diags);
        return;
    }
    if (const auto p = std::get_if<file_channel>(&channel)) {
        setup(name, c, *p, diags);
        return;
    }
    if (auto p = std::get_if<reference_channel>(&channel)) {
        diags << "found reference channel, referencing " << p->other << "\n";
        if (p->other) {
            setup(name, c, *(p->other), diags);
        }
        return;
    }
    diags << "found UNKNOWN channel type!!!!\n";
}

[[noreturn]]
auto exec_child(const system_name& name,
                 const executable_system& exe_proto,
                 char * const argv[],
                 char * const envp[],
                 const std::span<const connection>& connections,
                 const std::span<channel>& channels,
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
        const auto i = static_cast<std::size_t>(&connection - connections.data());
        setup(name, connection, channels[i], diags);
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
    const auto ec = os_error_code(errno);
    diags << "execve of " << exe_proto.executable_file << "failed: " << ec << "\n";
    diags.flush();
    ::_exit(1);
}

auto make_child(const system_name& name,
                const system& sys,
                const std::span<const connection>& connections,
                const std::span<channel>& channels,
                std::ostream& diags) -> instance;

auto make_child(const system_name& name,
                const custom_system& system,
                const std::span<const connection>& connections,
                const std::span<channel>& channels,
                std::ostream& diags) -> instance
{
    instance result;
    result.pid = no_process_id;
    for (auto&& entry: system.subsystems) {
        auto kid = make_child(entry.first, entry.second,
                              connections, channels, diags);
        result.children.emplace(entry.first, std::move(kid));
    }
    for (auto&& connection: system.connections) {
        result.channels.push_back(make_channel(name, system, connection,
                                               connections,
                                               channels));
    }
    return result;
}

auto make_child(const system_name& name,
                const system& sys,
                const std::span<const connection>& connections,
                const std::span<channel>& channels,
                std::ostream& diags) -> instance
{
    if (const auto p = std::get_if<executable_system>(&sys)) {
        return {no_process_id};
    }
    if (const auto p = std::get_if<custom_system>(&sys)) {
        return make_child(name, *p, connections, channels, diags);
    }
    diags << "parent: unrecognized system type for " << name << "\n";
    return {};
}

auto fork_child(const system_name& name,
                const executable_system& executable,
                instance& child,
                process_id& pgrp,
                const std::span<const connection>& connections,
                const std::span<channel>& channels,
                std::ostream& diags) -> void
{
    child.diags = temporary_fstream();
    auto arg_buffers = make_arg_bufs(executable.arguments,
                                     executable.executable_file);
    auto env_buffers = make_arg_bufs(executable.environment);
    auto argv = make_argv(arg_buffers);
    auto envp = make_argv(env_buffers);
    child.pid = process_id{::fork()};
    switch (child.pid) {
    case invalid_process_id:
        diags << "fork failed: " << os_error_code(errno) << "\n";
        return;
    case process_id{0}: { // child process
        // Have to be especially careful here!
        // From https://man7.org/linux/man-pages/man2/fork.2.html:
        // "child can safely call only async-signal-safe functions
        //  (see signal-safety(7)) until such time as it calls execve(2)."
        // See https://man7.org/linux/man-pages/man7/signal-safety.7.html
        // for "functions required to be async-signal-safe by POSIX.1".
        if (::setpgid(0, -int(pgrp)) == -1) {
            child.diags << "setpgid failed(0, " << -int(pgrp) << "): ";
            child.diags << os_error_code(errno);
            child.diags << "\n";
        }
        make_substitutions(argv);
        // NOTE: the following does not return!
        exec_child(name, executable, argv.data(), envp.data(),
                   connections, channels, child.diags);
    }
    default: // case for the spawning/parent process
        diags << "child '" << name << "' started as pid " << child.pid << "\n";
        if (pgrp == no_process_id) {
            pgrp = process_id(-int(child.pid));
        }
        break;
    }
}

auto fork_child(const system_name& name,
                const system& sys,
                instance& child,
                process_id& pgrp,
                const std::span<const connection>& connections,
                const std::span<channel>& channels,
                std::ostream& diags) -> void
{
    if (const auto p = std::get_if<executable_system>(&sys)) {
        fork_child(name, *p, child, pgrp, connections, channels, diags);
        return;
    }
    if (const auto p = std::get_if<custom_system>(&sys)) {
        for (auto&& entry: p->subsystems) {
            const auto found = child.children.find(entry.first);
            if (found == child.children.end()) {
                diags << "can't find child instance for " << entry.first;
                diags << "!\n";
                continue;
            }
            fork_child(entry.first, entry.second, found->second, pgrp,
                       connections, channels, diags);
        }
        return;
    }
    diags << "parent: unrecognized system type for " << name << "\n";
}

}

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&
{
    os << "instance{";
    os << ".pid=" << value.pid;
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

auto pretty_print(std::ostream& os, const instance& value) -> void
{
    os << "{\n";
    os << "  .pid=" << value.pid << ",\n";
    if (value.children.empty()) {
        os << "  .children={},\n";
    }
    else {
        os << "  .children={\n";
        for (auto&& entry: value.children) {
            os << "    {\n";
            os << "      .first='" << entry.first << "',\n";
            os << "      .second=";
            {
                const detail::indenting_ostreambuf child_indent{os, {6, false}};
                pretty_print(os, entry.second);
            }
            os << "    },\n";
        }
        os << "  },\n";
    }
    if (value.channels.empty()) {
        os << "  .channels={}\n";
    }
    else {
        os << "  .channels={\n";
        for (auto&& channel: value.channels) {
            os << "    " << channel << " (" << &channel << "),\n";
        }
        os << "  }\n";
    }
    os << "}\n";
}

auto total_descendants(const instance& object) -> std::size_t
{
    auto result = std::size_t{0};
    for (auto&& child: object.children) {
        result += total_descendants(child.second) + 1u;
    }
    return result;
}

auto total_channels(const instance& object) -> std::size_t
{
    auto result = std::size(object.channels);
    for (auto&& child: object.children) {
        result += total_channels(child.second);
    }
    return result;
}

auto instantiate(const system_name& name, const custom_system& system,
                 std::ostream& diags,
                 const std::span<const connection>& parent_connections,
                 const std::span<channel>& parent_channels)
    -> instance
{
    instance result;
    result.pid = no_process_id;
    result.channels.reserve(size(system.connections));
    for (auto&& connection: system.connections) {
        result.channels.push_back(make_channel(name, system, connection,
                                               parent_connections,
                                               parent_channels));
    }
    // Have to create all the subsystem instances before forking any!
    for (auto&& entry: system.subsystems) {
        const auto& sub_name = entry.first;
        const auto& sub_system = entry.second;
        auto kid = make_child(sub_name, sub_system,
                              system.connections, result.channels, diags);
        result.children.emplace(sub_name, std::move(kid));
    }
    for (auto&& entry: system.subsystems) {
        const auto found = result.children.find(entry.first);
        if (found == result.children.end()) {
            diags << "can't find child instance for " << entry.first;
            diags << "!\n";
            continue;
        }
        fork_child(entry.first, entry.second, found->second, result.pid,
                   system.connections, result.channels, diags);
    }
    // Only now, after making child processes,
    // close parent side of pipe_channels...
    for (auto&& channel: result.channels) {
        const auto i = &channel - result.channels.data();
        const auto& connection = system.connections[i];
        if (const auto p = std::get_if<pipe_channel>(&channel)) {
            static constexpr auto iowidth = 5;
            const auto ends = make_endpoints<system_endpoint>(connection);
            if (ends[0] && (ends[0]->address != system_name{})) {
                const auto pio = pipe_channel::io::write;
                diags << "parent '" << name << "': closing ";
                diags << std::setw(iowidth) << pio << " side of ";
                diags << connection << " " << *p << "\n";
                p->close(pio, diags);
            }
            if (ends[1] && (ends[1]->address != system_name{})) {
                const auto pio = pipe_channel::io::read;
                diags << "parent '" << name << "': closing ";
                diags << std::setw(iowidth) << pio << " side of ";
                diags << connection << " " << *p << "\n";
                p->close(pio, diags);
            }
            continue;
        }
        if (const auto p = std::get_if<reference_channel>(&channel)) {
            diags << "parent '" << name << "': channel[" << i << "] is reference";
            if (p->other) {
                diags << " to " << *(p->other);
            }
            diags << ".\n";
        }
    }
    return result;
}

}
