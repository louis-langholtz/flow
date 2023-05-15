#include <concepts> // for std::convertible_to
#include <cstdlib> // for std::exit, EXIT_FAILURE
#include <cstring> // for std::strcmp
#include <sstream> // for std::ostringstream

#include <unistd.h> // for getpid, setpgid
#include <fcntl.h> // for ::open

#include "pipe_registry.hpp"

#include "flow/indenting_ostreambuf.hpp"
#include "flow/instance.hpp"
#include "flow/os_error_code.hpp"
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
    if (const auto p = std::get_if<instance::custom>(&root.info)) {
        for (auto&& entry: p->children) {
            if (&entry.second == &child) {
                return &root;
            }
            if (const auto found = find_parent(entry.second, child)) {
                return found;
            }
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

template <std::convertible_to<channel> T>
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
                char * const *argv,
                char * const *envp,
                std::ostream& diags) -> void
{
    diags.flush();
    ::execve(path.c_str(), argv, envp);
    const auto ec = os_error_code(errno);
    diags << "execve of '" << path.native() << "' failed: " << ec << "\n";
    diags.flush();
    exit(exit_failure_code);
}

auto confirm_closed(const system_name& name,
                    const descriptor_map& descriptors,
                    const std::span<const connection>& connections) -> void
{
    for (auto&& entry: descriptors) {
        const auto look_for = system_endpoint{name, entry.first};
        if (!find_index(connections, look_for)) {
            std::ostringstream os;
            os << "missing connection for " << look_for;
            throw std::invalid_argument{os.str()};
        }
    }
}

auto make_child(instance& parent,
                const system_name& name,
                const system& system,
                const std::span<const connection>& connections) -> instance
{
    instance result;
    confirm_closed(name, system.descriptors, connections);
    result.environment = parent.environment;
    for (auto&& entry: system.environment) {
        result.environment[entry.first] = entry.second;
    }
    if (const auto p = std::get_if<system::executable>(&system.info)) {
        if (p->file.empty()) {
            std::ostringstream os;
            os << "cannot instantiate executable system '";
            os << name;
            os << "': no executable file specified - it's empty";
            throw std::invalid_argument{os.str()};
        }
        instance::forked info;
        info.diags = ext::temporary_fstream();
        result.info = std::move(info);
    }
    else if (const auto p = std::get_if<system::custom>(&system.info)) {
        result.info = instance::custom{};
        auto& parent_info = std::get<instance::custom>(parent.info);
        auto& info = std::get<instance::custom>(result.info);
        for (auto&& connection: p->connections) {
            info.channels.push_back(make_channel(name, system, connection,
                                                 connections,
                                                 parent_info.channels));
        }
        for (auto&& entry: p->subsystems) {
            auto kid = make_child(result, entry.first, entry.second,
                                  p->connections);
            info.children.emplace(entry.first, std::move(kid));
        }
    }
    else {
        std::ostringstream os;
        os << "parent: unrecognized system type for " << name << "\n";
        throw std::logic_error{os.str()};
    }
    return result;
}

auto change_directory(const std::filesystem::path& path, std::ostream& diags)
    -> void
{
    auto ec = std::error_code{};
    std::filesystem::current_path(path, ec);
    if (ec) {
        diags << "chdir " << path << " failed: ";
        write(diags, ec);
        diags << "\n";
        exit(exit_failure_code);
    }
}

auto close_unused_descriptors(const system_name& name,
                              const std::span<const connection>& conns)
    -> void
{
    auto using_stdin = false;
    auto using_stdout = false;
    auto using_stderr = false;
    for (auto&& conn: conns) {
        const auto ends = make_endpoints<system_endpoint>(conn);
        for (auto&& end: ends) {
            if (end && end->address == name) {
                for (auto&& descriptor: end->descriptors) {
                    switch (descriptor) {
                    case descriptors::stdin_id:
                        using_stdin = true;
                        break;
                    case descriptors::stdout_id:
                        using_stdout = true;
                        break;
                    case descriptors::stderr_id:
                        using_stderr = true;
                        break;
                    }
                }
            }
        }
    }
    if (!using_stdin) {
        ::close(int(descriptors::stdin_id));
    }
    if (!using_stdout) {
        ::close(int(descriptors::stdout_id));
    }
    if (!using_stderr) {
        ::close(int(descriptors::stderr_id));
    }
}

auto close_pipes_except(instance& root,
                        instance& child) -> void
{
    if (const auto parent = find_parent(root, child)) {
        auto& parent_info = std::get<instance::custom>(parent->info);
        auto& child_info = std::get<instance::forked>(child.info);
        for (auto&& pipe: the_pipe_registry().pipes) {
            if (!is_channel_for(parent_info.channels, pipe)) {
                pipe->close(pipe_channel::io::read, child_info.diags);
                pipe->close(pipe_channel::io::write, child_info.diags);
            }
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
    auto& child_info = std::get<instance::forked>(child.info);
    const auto max_index = size(connections);
    assert(max_index == size(channels));
    for (auto index = 0u; index < max_index; ++index) {
        const auto chan_p = fully_deref(&channels[index]);
        assert(chan_p != nullptr);
        if (const auto pipe_p = std::get_if<pipe_channel>(chan_p)) {
            setup(name, connections[index], *pipe_p, child_info.diags);
            continue;
        }
        if (const auto file_p = std::get_if<file_channel>(chan_p)) {
            setup(name, connections[index], *file_p, child_info.diags);
            continue;
        }
        child_info.diags << "found UNKNOWN channel type!!!!\n";
    }
    close_unused_descriptors(name, connections);
    close_pipes_except(root, child);
}

auto find_file(const std::filesystem::path& file,
               const env_value& path)
    -> std::optional<std::filesystem::path>
{
    static constexpr auto delimiter = ':';
    auto ec = std::error_code{};
    auto last = std::size_t{};
    auto next = std::size_t{};
    while ((next = path.get().find(delimiter, last)) != std::string::npos) {
        const auto dir = path.get().substr(last, next - last);
        if (!dir.empty()) {
            const auto full_path = dir / file;
            if (exists(full_path, ec) && !ec) {
                return full_path;
            }
        }
        last = next + 1u;
    }
    const auto dir = path.get().substr(last);
    if (!dir.empty()) {
        const auto full_path = dir / file;
        if (exists(full_path, ec) && !ec) {
            return full_path;
        }
    }
    return {};
}

auto fork_child(const system_name& name,
                const system::executable& exe,
                instance& child,
                reference_process_id& pgrp,
                const std::span<const connection>& connections,
                const std::span<channel>& channels,
                instance& root,
                std::ostream& diags) -> void
{
    auto exe_path = exe.file;
    if (exe_path.empty()) {
        diags << "no file specified to execute\n";
        return;
    }
    if (exe_path.is_relative() && !exe_path.has_parent_path()) {
        auto path_env_value = static_cast<const env_value*>(nullptr);
        if (const auto it = child.environment.find("PATH");
            it != child.environment.end()) {
            path_env_value = &(it->second);
        }
        if (!path_env_value) {
            diags << "no PATH to find file " << exe_path << "\n";
            return;
        }
        const auto found = find_file(exe_path, *path_env_value);
        if (!found) {
            diags << "no such file in PATH as " << exe_path << "\n";
            return;
        }
        exe_path = *found;
    }
    auto& child_info = std::get<instance::forked>(child.info);
    auto arg_buffers = make_arg_bufs(exe.arguments, exe_path);
    auto env_buffers = make_arg_bufs(child.environment);
    auto argv = make_argv(arg_buffers);
    auto envp = make_argv(env_buffers);
    auto owning_pid = owning_process_id::fork();
    const auto pid = reference_process_id(owning_pid);
    child_info.state = std::move(owning_pid);
    switch (pid) {
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
        if (::setpgid(0, int(pgrp)) == -1) {
            child_info.diags << "setpgid failed(0, ";
            child_info.diags << pgrp;
            child_info.diags << "): ";
            child_info.diags << os_error_code(errno);
            child_info.diags << "\n";
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
        if (!exe.working_directory.empty()) {
            change_directory(exe.working_directory, child_info.diags);
        }
        // NOTE: the following does not return!
        exec_child(exe_path, argv.data(), envp.data(), child_info.diags);
    }
    default: // case for the spawning/parent process
        if (pgrp == no_process_id) {
            pgrp = reference_process_id(int(pid));
        }
        break;
    }
}

auto fork_executables(const system::custom& system,
                      instance& object,
                      instance& root,
                      std::ostream& diags) -> void
{
    auto& info = std::get<instance::custom>(object.info);
    for (auto&& entry: system.subsystems) {
        const auto& name = entry.first;
        const auto& subsystem = entry.second;
        const auto found = info.children.find(name);
        if (found == info.children.end()) {
            diags << "can't find child instance for " << name << "!\n";
            continue;
        }
        if (const auto p = std::get_if<system::executable>(&subsystem.info)) {
            fork_child(name, *p, found->second, info.pgrp, system.connections,
                       info.channels, root, diags);
            continue;
        }
        if (const auto p = std::get_if<system::custom>(&subsystem.info)) {
            fork_executables(*p, found->second, root, diags);
            continue;
        }
        diags << "Detected unknown system type - skipping!\n";
    }
}

auto close_internal_ends(const connection& connection,
                         pipe_channel& channel,
                         std::ostream& diags) -> void
{
    static constexpr auto iowidth = 5;
    const auto ends = make_endpoints<system_endpoint>(connection);
    if (ends[0] && (ends[0]->address != system_name{})) {
        const auto pio = pipe_channel::io::write;
        diags << "parent: closing ";
        diags << std::setw(iowidth) << pio << " side of ";
        diags << connection << " " << channel << "\n";
        channel.close(pio, diags);
    }
    if (ends[1] && (ends[1]->address != system_name{})) {
        const auto pio = pipe_channel::io::read;
        diags << "parent: closing ";
        diags << std::setw(iowidth) << pio << " side of ";
        diags << connection << " " << channel << "\n";
        channel.close(pio, diags);
    }
}

}

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&
{
    os << "instance{";
    os << ".environment=" << value.environment;
    if (const auto p = std::get_if<instance::custom>(&value.info)) {
        os << ".pgrp=" << p->pgrp;
        os << ",.children={";
        for (auto&& entry: p->children) {
            if (&entry != &(*p->children.begin())) {
                os << ",";
            }
            os << "{";
            os << ".first=" << entry.first;
            os << ",.second=" << entry.second;
            os << "}";
        }
        os << "}";
        os << ",.channels={";
        for (auto&& elem: p->channels) {
            if (&elem != &(*p->channels.begin())) {
                os << ",";
            }
            os << elem;
        }
        os << "}";
    }
    else if (const auto p = std::get_if<instance::forked>(&value.info)) {
        os << ".state=" << p->state;
    }
    os << "}";
    return os;
}

auto pretty_print(std::ostream& os, const instance& value) -> void
{
    os << "{\n";
    if (const auto p = std::get_if<instance::custom>(&value.info)) {
        os << "  .pgrp=" << p->pgrp << ",\n";
        if (p->children.empty()) {
            os << "  .children={},\n";
        }
        else {
            os << "  .children={\n";
            for (auto&& entry: p->children) {
                os << "    {\n";
                os << "      .first='" << entry.first << "',\n";
                os << "      .second=";
                {
                    const auto opts = detail::indenting_ostreambuf_options{
                        6, false
                    };
                    const detail::indenting_ostreambuf child_indent{os, opts};
                    pretty_print(os, entry.second);
                }
                os << "    },\n";
            }
            os << "  },\n";
        }
        if (p->channels.empty()) {
            os << "  .channels={}\n";
        }
        else {
            os << "  .channels={\n";
            for (auto&& channel: p->channels) {
                os << "    " << channel << " (" << &channel << "),\n";
            }
            os << "  }\n";
        }
    }
    else if (const auto p = std::get_if<instance::forked>(&value.info)) {
        os << "  .state=" << p->state;
    }
    os << "}\n";
}

auto get_reference_process_id(const instance::forked& object)
    -> reference_process_id
{
    if (const auto p = std::get_if<owning_process_id>(&object.state)) {
        return reference_process_id(*p);
    }
    return invalid_process_id;
}

auto get_reference_process_id(const std::vector<system_name>& names,
                              const instance& object)
    -> reference_process_id
{
    auto* info = &object.info;
    for (auto comps = names; !empty(comps); comps.pop_back()) {
        const auto& comp = comps.back();
        auto found = false;
        if (const auto p = std::get_if<instance::custom>(info)) {
            for (auto&& entry: p->children) {
                if (entry.first == comp) {
                    info = &entry.second.info;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            throw std::invalid_argument{"no such component"};
        }
    }
    if (const auto p = std::get_if<instance::forked>(info)) {
        return get_reference_process_id(*p);
    }
    throw std::invalid_argument{"wrong instance type found"};
}

auto total_descendants(const instance& object) -> std::size_t
{
    auto result = std::size_t{0};
    if (const auto p = std::get_if<instance::custom>(&object.info)) {
        for (auto&& child: p->children) {
            result += total_descendants(child.second) + 1u;
        }
    }
    return result;
}

auto total_channels(const instance& object) -> std::size_t
{
    auto result = std::size_t{};
    if (const auto p = std::get_if<instance::custom>(&object.info)) {
        result += std::size(p->channels);
        for (auto&& child: p->children) {
            result += total_channels(child.second);
        }
    }
    return result;
}

auto get_wait_status(const instance& object) -> wait_status
{
    if (const auto q = std::get_if<instance::forked>(&object.info)) {
        if (const auto r = std::get_if<wait_status>(&(q->state))) {
            return *r;
        }
    }
    return {wait_unknown_status{}};
}

auto instantiate(const system& system,
                 std::ostream& diags,
                 environment_map env)
    -> instance
{
    instance result;
    for (auto&& entry: system.environment) {
        env[entry.first] = entry.second;
    }
    result.environment = std::move(env);
    if (const auto p = std::get_if<system::executable>(&system.info)) {
        confirm_closed({}, system.descriptors, {});
        if (p->file.empty()) {
            throw std::invalid_argument{"no executable file - it's empty"};
        }
        result.info = instance::forked{};
        auto& info = std::get<instance::forked>(result.info);
        info.diags = ext::temporary_fstream();
        auto pgrp = no_process_id;
        fork_child({}, *p, result, pgrp, {}, {}, result, diags);
    }
    else if (const auto p = std::get_if<system::custom>(&system.info)) {
        confirm_closed({}, system.descriptors, p->connections);
        result.info = instance::custom{};
        auto& info = std::get<instance::custom>(result.info);
        for (auto&& entry: system.descriptors) {
            const auto look_for = system_endpoint{{}, entry.first};
            if (!find_index(p->connections, look_for)) {
                throw std::invalid_argument{"enclosing endpoint not connected"};
            }
        }
        info.channels.reserve(size(p->connections));
        for (auto&& connection: p->connections) {
            info.channels.push_back(make_channel({}, system, connection,
                                                 {}, {}));
        }
        // Create all the subsystem instances before forking any!
        for (auto&& entry: p->subsystems) {
            const auto& sub_name = entry.first;
            const auto& sub_system = entry.second;
            auto kid = make_child(result, sub_name, sub_system, p->connections);
            info.children.emplace(sub_name, std::move(kid));
        }
        fork_executables(*p, result, result, diags);
        // Only now, after making child processes,
        // close parent side of pipe_channels...
        const auto max_i = size(info.channels);
        for (auto i = 0u; i < max_i; ++i) {
            auto& channel = info.channels[i];
            const auto& connection = p->connections[i];
            if (const auto q = std::get_if<pipe_channel>(&channel)) {
                close_internal_ends(connection, *q, diags);
                continue;
            }
        }
    }
    return result;
}

}
