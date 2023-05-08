#include <cstdlib> // for std::exit, EXIT_FAILURE
#include <cstring> // for std::strcmp
#include <sstream> // for std::ostringstream

#include <unistd.h> // for getpid, setpgid
#include <fcntl.h> // for ::open

#include "indenting_ostreambuf.hpp"
#include "os_error_code.hpp"
#include "pipe_registry.hpp"

#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace flow {

namespace {

constexpr auto exit_failure_code = EXIT_FAILURE;

/// @brief Exit the running process.
/// @note This exists to abstractly wrap the actual exit function used.
/// @note This also helps isolate clang-tidy issues to this sole function.
[[noreturn]]
auto exit(int exit_code) -> void
{
    std::exit(exit_code); // NOLINT(concurrency-mt-unsafe)
}

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

auto find_parent(const instance& root, const instance& child)
    -> const instance*
{
    for (auto&& entry: root.children) {
        if (&entry.second == &child) {
            return &root;
        }
        if (const auto found = find_parent(entry.second, child)) {
            return found;
        }
    }
    return nullptr;
}

template <class T>
auto fully_deref(T&& chan_p)
    -> decltype(std::get_if<reference_channel>(chan_p)->other)
{
    using ref_type = decltype(std::get_if<reference_channel>(chan_p));
    auto ref_p = static_cast<ref_type>(nullptr);
    while ((ref_p = std::get_if<reference_channel>(chan_p)) != nullptr) {
        chan_p = ref_p->other;
    }
    return chan_p;
}

template <class T>
auto is_channel_for(const std::span<const channel>& channels,
                    const T *key) -> bool
{
    for (auto&& channel: channels) {
        auto chan_p = &channel;
        using ref_type = decltype(std::get_if<reference_channel>(chan_p));
        auto ref_p = static_cast<ref_type>(nullptr);
        while ((ref_p = std::get_if<reference_channel>(chan_p)) != nullptr) {
            if constexpr (std::is_same_v<T, reference_channel>) {
                if (ref_p == key) {
                    return true;
                }
            }
            chan_p = ref_p->other;
        }
        if constexpr (!std::is_same_v<T, reference_channel>) {
            if (const auto q = std::get_if<T>(chan_p)) {
                if (q == key) {
                    return true;
                }
            }
        }
    }
    return false;
}

auto close(pipe_channel& p, pipe_channel::io side,
           const system_name& name, const connection& c,
           std::ostream& diags)
    -> void
{
    diags << name << " " << c << " " << p;
    diags << ", close  " << side << "-side\n";
    if (!p.close(side, diags)) { // close unused end
        exit(exit_failure_code);
    }
}

auto dup2(pipe_channel& p, pipe_channel::io side, descriptor_id id,
          const system_name& name, const connection& c,
          std::ostream& diags)
    -> void
{
    diags << name << " " << c << " " << p;
    diags << ", dup " << side << "-side to ";
    diags << id << "\n";
    if (!p.dup(side, id, diags)) {
        exit(exit_failure_code);
    }
}

auto setup(const system_name& name,
           const connection& conn,
           pipe_channel& p,
           std::ostream& diags) -> void
{
    using io = pipe_channel::io;
    const auto ends = make_endpoints<system_endpoint>(conn);
    if (!ends[0] && !ends[1]) {
        diags << "connection has no system_endpoint: " << conn << "\n";
        return;
    }
    if ((!ends[0] || (ends[0]->address != name)) &&
        (!ends[1] || (ends[1]->address != name))) {
        diags << name << " (unaffiliation) " << conn;
        diags << " " << p << ", close in & out setup\n";
        close(p, io::read, name, conn, diags);
        close(p, io::write, name, conn, diags);
        return;
    }
    if (ends[0]) { // src
        if (ends[0]->address == name) {
            close(p, io::read, name, conn, diags);
            for (auto&& descriptor: ends[0]->descriptors) {
                dup2(p, io::write, descriptor, name, conn, diags);
            }
        }
    }
    if (ends[1]) { // dst
        if (ends[1]->address == name) {
            close(p, io::write, name, conn, diags);
            for (auto&& descriptor: ends[1]->descriptors) {
                dup2(p, io::read, descriptor, name, conn, diags);
            }
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
    assert(file_ends[0] || file_ends[1]);
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
            exit(exit_failure_code);
        }
        for (auto&& descriptor: op->descriptors) {
            if (::dup2(fd, int(descriptor)) == -1) {
                diags << name << " " << c;
                diags << ", dup2(" << fd << "," << descriptor << ") failed: ";
                diags << os_error_code(errno) << "\n";
                exit(exit_failure_code);
            }
        }
    }
}

auto setup(const system_name& name,
           const unidirectional_connection& conn,
           file_channel& chan,
           std::ostream& diags) -> void
{
    const auto sys_ends = make_endpoints<system_endpoint>(conn);
    assert(sys_ends[0] || sys_ends[1]);
    const auto op = [&sys_ends,&name](){
        for (auto&& sys_end: sys_ends) {
            if (sys_end && sys_end->address == name) {
                return sys_end;
            }
        }
        return static_cast<const system_endpoint*>(nullptr);
    }();
    if (op) {
        const auto flags = to_open_flags(chan.io);
        const auto mode = 0600;
        const auto fd = ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
                               chan.path.c_str(), flags, mode);
        if (fd == -1) {
            static constexpr auto mode_width = 5;
            diags << name << " " << conn;
            diags << ", open file " << chan.path << " with mode ";
            diags << std::oct << std::setfill('0') << std::setw(mode_width);
            diags << flags;
            diags << " failed: " << os_error_code(errno) << "\n";
            exit(exit_failure_code);
        }
        for (auto&& descriptor: op->descriptors) {
            if (::dup2(fd, int(descriptor)) == -1) {
                diags << name << " " << conn;
                diags << ", dup2(" << fd << "," << descriptor << ") failed: ";
                diags << os_error_code(errno) << "\n";
                exit(exit_failure_code);
            }
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

[[noreturn]]
auto exec_child(const std::filesystem::path& path,
                char * const argv[],
                char * const envp[],
                std::ostream& diags) -> void
{
    diags.flush();
    ::execve(path.c_str(), argv, envp);
    const auto ec = os_error_code(errno);
    diags << "execve of " << path << "failed: " << ec << "\n";
    diags.flush();
    exit(exit_failure_code);
}

auto make_child(instance& parent,
                const system_name& name,
                const system& system,
                const std::span<const connection>& connections,
                std::ostream& diags) -> instance
{
    instance result;
    for (auto&& entry: system.descriptors) {
        const auto look_for = system_endpoint{name, entry.first};
        if (!find_index(connections, look_for)) {
            std::ostringstream os;
            os << "missing connection for " << look_for;
            throw std::invalid_argument{os.str()};
        }
    }
    result.environment = parent.environment;
    for (auto&& entry: system.environment) {
        result.environment[entry.first] = entry.second;
    }
    result.pid = no_process_id;
    if (const auto p = std::get_if<system::executable>(&system.info)) {
        result.diags = temporary_fstream();
    }
    else if (const auto p = std::get_if<system::custom>(&system.info)) {
        for (auto&& connection: p->connections) {
            result.channels.push_back(make_channel(name, system, connection,
                                                   connections,
                                                   parent.channels));
        }
        for (auto&& entry: p->subsystems) {
            auto kid = make_child(result, entry.first, entry.second,
                                  p->connections, diags);
            result.children.emplace(entry.first, std::move(kid));
        }
    }
    else {
        diags << "parent: unrecognized system type for " << name << "\n";
    }
    return result;
}

auto change_directory(const std::filesystem::path& path, std::ostream& diags)
    -> bool
{
    auto ec = std::error_code{};
    std::filesystem::current_path(path, ec);
    if (ec) {
        diags << "chdir " << path << " failed: ";
        write(diags, ec);
        diags << "\n";
        exit(exit_failure_code);
    }
    return true;
}

auto close_pipes_except(instance& root,
                        instance& child) -> void
{
    const auto parent = find_parent(root, child);
    assert(parent != nullptr);
    for (auto&& pipe: the_pipe_registry().pipes) {
        if (!is_channel_for(parent->channels, pipe)) {
            pipe->close(pipe_channel::io::read, child.diags);
            pipe->close(pipe_channel::io::write, child.diags);
        }
    }
}

auto setup(instance& root,
           const system_name& name,
           const std::span<const connection>& connections,
           const std::span<channel>& channels,
           instance& child) -> void
{
    // close & dup the channels needed for name, and close those that aren't
    const auto max_index = size(connections);
    assert(max_index == size(channels));
    for (auto index = 0u; index < max_index; ++index) {
        const auto chan_p = fully_deref(&channels[index]);
        assert(chan_p != nullptr);
        if (const auto pipe_p = std::get_if<pipe_channel>(chan_p)) {
            setup(name, connections[index], *pipe_p, child.diags);
            continue;
        }
        if (const auto file_p = std::get_if<file_channel>(chan_p)) {
            setup(name, connections[index], *file_p, child.diags);
            continue;
        }
        child.diags << "found UNKNOWN channel type!!!!\n";
    }
    close_pipes_except(root, child);
}

auto fork_child(const system_name& name,
                const system::executable& system,
                instance& child,
                process_id& pgrp,
                const std::span<const connection>& connections,
                const std::span<channel>& channels,
                instance& root,
                std::ostream& diags) -> void
{
    auto arg_buffers = make_arg_bufs(system.arguments,
                                     system.executable_file);
    auto env_buffers = make_arg_bufs(child.environment);
    auto argv = make_argv(arg_buffers);
    auto envp = make_argv(env_buffers);
    child.pid = owning_process_id::fork();
    switch (to_reference_process_id(child.pid)) {
    case invalid_process_id:
        diags << "fork failed: " << os_error_code(errno) << "\n";
        return;
    case no_process_id: { // child process
        // Have to be especially careful here!
        // From https://man7.org/linux/man-pages/man2/fork.2.html:
        // "child can safely call only async-signal-safe functions
        //  (see signal-safety(7)) until such time as it calls execve(2)."
        // See https://man7.org/linux/man-pages/man7/signal-safety.7.html
        // for "functions required to be async-signal-safe by POSIX.1".
        if (::setpgid(0, -int(to_reference_process_id(pgrp))) == -1) {
            child.diags << "setpgid failed(0, ";
            child.diags << -int(to_reference_process_id(pgrp)) << "): ";
            child.diags << os_error_code(errno);
            child.diags << "\n";
        }
        make_substitutions(argv);
        // Deal with:
        // unidirectional_connection{
        //   system_endpoint{ls_process_name, flow::descriptor_id{1}},
        //   system_endpoint{cat_process_name, flow::descriptor_id{0}}
        // }
        //
        // Also:
        // Close file descriptors inherited by child that it's not using.
        // See: https://stackoverflow.com/a/7976880/7410358
        setup(root, name, connections, channels, child);
        // NOTE: child.diags streams opened close-on-exec, so no need
        //   to close them.
        if (!system.working_directory.empty()) {
            change_directory(system.working_directory, child.diags);
        }
        // NOTE: the following does not return!
        exec_child(system.executable_file, argv.data(), envp.data(),
                   child.diags);
    }
    default: // case for the spawning/parent process
        diags << "child '" << name << "' started as pid " << child.pid << "\n";
        if (pgrp == no_process_id) {
            pgrp = reference_process_id(-int(to_reference_process_id(child.pid)));
        }
        break;
    }
}

auto fork_executables(const system::custom& system,
                      instance& object,
                      instance& root,
                      std::ostream& diags) -> void
{
    for (auto&& entry: system.subsystems) {
        const auto& name = entry.first;
        const auto& subsystem = entry.second;
        const auto found = object.children.find(name);
        if (found == object.children.end()) {
            diags << "can't find child instance for " << name << "!\n";
            continue;
        }
        if (const auto p = std::get_if<system::executable>(&subsystem.info)) {
            fork_child(name, *p, found->second, object.pid, system.connections,
                       object.channels, root, diags);
            continue;
        }
        if (const auto p = std::get_if<system::custom>(&subsystem.info)) {
            fork_executables(*p, found->second, root, diags);
            continue;
        }
        diags << "Detected unknown system type - skipping!\n";
    }
}

auto close_internal_ends(const system_name& name,
                         const connection& connection,
                         pipe_channel& channel,
                         std::ostream& diags) -> void
{
    static constexpr auto iowidth = 5;
    const auto ends = make_endpoints<system_endpoint>(connection);
    if (ends[0] && (ends[0]->address != system_name{})) {
        const auto pio = pipe_channel::io::write;
        diags << "parent '" << name << "': closing ";
        diags << std::setw(iowidth) << pio << " side of ";
        diags << connection << " " << channel << "\n";
        channel.close(pio, diags);
    }
    if (ends[1] && (ends[1]->address != system_name{})) {
        const auto pio = pipe_channel::io::read;
        diags << "parent '" << name << "': closing ";
        diags << std::setw(iowidth) << pio << " side of ";
        diags << connection << " " << channel << "\n";
        channel.close(pio, diags);
    }
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

auto instantiate(const system_name& name,
                 const system& system,
                 std::ostream& diags,
                 environment_map env)
    -> instance
{
    instance result;
    for (auto&& entry: system.environment) {
        env[entry.first] = entry.second;
    }
    result.environment = std::move(env);
    result.pid = no_process_id;
    if (const auto p = std::get_if<system::executable>(&system.info)) {
        fork_child(name, *p, result, result.pid, {}, {}, result, diags);
    }
    else if (const auto p = std::get_if<system::custom>(&system.info)) {
        for (auto&& entry: system.descriptors) {
            const auto look_for = system_endpoint{{}, entry.first};
            if (!find_index(p->connections, look_for)) {
                throw std::invalid_argument{"enclosing endpoint not connected"};
            }
        }
        result.channels.reserve(size(p->connections));
        for (auto&& connection: p->connections) {
            result.channels.push_back(make_channel(name, system, connection,
                                                   {}, {}));
        }
        // Create all the subsystem instances before forking any!
        for (auto&& entry: p->subsystems) {
            const auto& sub_name = entry.first;
            const auto& sub_system = entry.second;
            auto kid = make_child(result, sub_name, sub_system,
                                  p->connections, diags);
            result.children.emplace(sub_name, std::move(kid));
        }
        fork_executables(*p, result, result, diags);
        // Only now, after making child processes,
        // close parent side of pipe_channels...
        const auto max_i = size(result.channels);
        for (auto i = 0u; i < max_i; ++i) {
            auto& channel = result.channels[i];
            const auto& connection = p->connections[i];
            if (const auto q = std::get_if<pipe_channel>(&channel)) {
                close_internal_ends(name, connection, *q, diags);
                continue;
            }
        }
    }
    return result;
}

}
