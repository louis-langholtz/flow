#include <concepts> // for std::convertible_to
#include <csignal>
#include <cstdlib> // for std::exit, EXIT_FAILURE
#include <cstring> // for std::strcmp
#include <iomanip> // for std::setfill
#include <sstream> // for std::ostringstream

#include <fcntl.h> // for ::open
#include <pthread.h>
#include <unistd.h> // for getpid, setpgid

#include "ext/expected.hpp"

#include "flow/instantiate.hpp"
#include "flow/utility.hpp"

#include "pipe_registry.hpp"

namespace flow {

namespace {

constexpr auto exit_failure_code = EXIT_FAILURE;

/// @brief Exit the running process.
/// @note This exists to abstractly wrap the actual exit function used
///   for exiting from a forked child process.
/// @note We have to be careful how we actually exit. As a C++ library,
///   we need to make sure our normal destructors actually don't get
///   called from this context! Otherwise, things like calling
///   std::future::get can block awaiting a thread that it thinks is
///   running when ::fork() actually doesn't copy threads. That leaves
///   user code stuck!
/// @note The alternative, is the child exec's. Which is for our purpose
///   closer to what we get when calling the underlying OS _exit call.
[[noreturn]]
auto exit(int exit_code) -> void
{
    ::_exit(exit_code); // NOLINT(concurrency-mt-unsafe)
}

auto dup2(int fd, reference_descriptor from) -> bool
{
    return ::dup2(fd, int(from)) != -1;
}

auto close(reference_descriptor d) -> bool
{
    return ::close(int(d)) != -1;
}

auto close(const std::set<port_id>& ports) -> void
{
    for (auto&& port: ports) {
        if (std::holds_alternative<reference_descriptor>(port)) {
            close(std::get<reference_descriptor>(port));
        }
    }
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
    if (const auto p = std::get_if<instance::system>(&root.info)) {
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

auto close(pipe_channel& p, pipe_channel::io side, const node_name& name,
           const link& c, std::ostream& diags) -> void
{
    diags << name << " " << c << " " << p;
    diags << ", close  " << side << "-side\n";
    if (!p.close(side, diags)) { // close unused end
        diags.flush();
        exit(exit_failure_code);
    }
}

auto dup2(pipe_channel& p, pipe_channel::io side, reference_descriptor id,
          const node_name& name, const link& c, std::ostream& diags) -> void
{
    diags << name << " " << c << " " << p;
    diags << ", dup " << side << "-side to ";
    diags << id << "\n";
    if (!p.dup(side, id, diags)) {
        diags.flush();
        exit(exit_failure_code);
    }
}

auto dup2(pipe_channel& pc,
          pipe_channel::io side,
          const std::set<port_id>& ports,
          const node_name& name,
          const link& conn,
          std::ostream& diags) -> void
{
    for (auto&& port: ports) {
        if (std::holds_alternative<reference_descriptor>(port)) {
            dup2(pc, side, std::get<reference_descriptor>(port),
                 name, conn, diags);
        }
    }
}

auto setup(const node_name& name,
           const link& conn,
           pipe_channel& p,
           std::ostream& diags) -> void
{
    using io = pipe_channel::io;
    const auto ends = make_endpoints<node_endpoint>(conn);
    if (!ends[0] && !ends[1]) {
        diags << "link has no node_endpoint: " << conn << "\n";
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
            dup2(p, io::write, ends[0]->ports, name, conn, diags);
        }
    }
    if (ends[1]) { // dst
        if (ends[1]->address == name) {
            close(p, io::write, name, conn, diags);
            dup2(p, io::read, ends[1]->ports, name, conn, diags);
        }
    }
}

auto to_open_flags(io_type direction) -> expected<int, std::string>
{
    switch (direction) {
    case io_type::in: return {O_RDONLY};
    case io_type::out: return {O_WRONLY};
    case io_type::bidir: return {O_RDWR};
    case io_type::none: return unexpected<std::string>{};
    }
    return unexpected<std::string>{"unrecognized io_type value"};
}

auto setup(const node_name& name,
           const link& conn,
           file_channel& chan,
           std::ostream& diags) -> void
{
    const auto sys_ends = make_endpoints<node_endpoint>(conn);
    assert(sys_ends[0] || sys_ends[1]);
    const auto op = [&sys_ends,&name](){
        for (auto&& sys_end: sys_ends) {
            if (sys_end && sys_end->address == name) {
                return sys_end;
            }
        }
        return static_cast<const node_endpoint*>(nullptr);
    }();
    if (op) {
        const auto flags = to_open_flags(chan.io);
        if (!flags) {
            if (!empty(flags.error())) {
                diags << name << " " << conn;
                diags << ", can't get needed open flags: ";
                diags << flags.error();
                diags << "\n";
                diags.flush();
                exit(exit_failure_code);
            }
            close(op->ports);
            return;
        }
        const auto mode = 0600;
        const auto fd = ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
                               chan.path.c_str(), *flags, mode);
        if (fd == -1) {
            static constexpr auto mode_width = 5;
            diags << name << " " << conn;
            diags << ", open file " << chan.path << " with mode ";
            diags << std::oct << std::setfill('0') << std::setw(mode_width);
            diags << *flags;
            diags << " failed: " << os_error_code(errno) << "\n";
            exit(exit_failure_code);
        }
        for (auto&& port: op->ports) {
            if (std::holds_alternative<reference_descriptor>(port)) {
                if (!dup2(fd, std::get<reference_descriptor>(port))) {
                    diags << name << " " << conn;
                    diags << ", dup2(" << fd << "," << port << ") failed: ";
                    diags << os_error_code(errno) << "\n";
                    exit(exit_failure_code);
                }
            }
        }
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
    diags << "execve of " << path << " failed: " << ec << "\n";
    diags.flush();
    exit(exit_failure_code);
}

auto confirm_closed(const node_name& name,
                    const port_map& ports,
                    const std::span<const link>& links,
                    const port_map& available) -> bool
{
    auto is_internally_closed = true;
    std::ostringstream os;
    auto prefix = "missing link for ";
    for (auto&& entry: ports) {
        if (!requires_link(entry)) {
            continue;
        }
        const auto look_for = node_endpoint{name, entry.first};
        if (find_index(links, look_for)) {
            continue;
        }
        if (const auto it = available.find(entry.first);
            it != available.end()) {
            if (entry.second.direction == it->second.direction) {
                is_internally_closed = false;
                continue;
            }
        }
        os << prefix << look_for;
        prefix = ", ";
    }
    if (const auto msg = os.str(); !empty(msg)) {
        throw invalid_port_map{msg};
    }
    return is_internally_closed;
}

[[noreturn]]
auto throw_has_no_filename(const std::filesystem::path& path,
                           const std::string& prefix = {}) -> void
{
    std::ostringstream os;
    os << prefix << path << " has no filename component";
    throw invalid_executable{os.str()};
}

auto make_child(instance& parent,
                const node_name& name,
                const node& node,
                const std::span<const link>& parent_links,
                const port_map& parent_ports) -> instance;

auto make_child(const node_name& name,
                const port_map& interface,
                const executable& implementation,
                const std::span<const link>& parent_links,
                const port_map& parent_ports) -> instance
{
    confirm_closed(name, interface, parent_links, parent_ports);
    if (!implementation.file.has_filename()) {
        std::ostringstream os;
        os << "cannot instantiate ";
        os << name;
        os << ": executable file path ";
        throw_has_no_filename(implementation.file, os.str());
    }
    return instance{instance::forked{ext::temporary_fstream(), {}}};
}

auto make_child(instance& parent,
                const node_name& name,
                const port_map& interface,
                const system& implementation,
                const std::span<const link>& parent_links,
                const port_map& parent_ports) -> instance
{
    instance result;
    const auto all_closed = confirm_closed(name, interface,
                                           parent_links, parent_ports);
    result.info = instance::system{};
    auto& parent_info = std::get<instance::system>(parent.info);
    auto& info = std::get<instance::system>(result.info);
    if (!all_closed) {
        info.pgrp = current_process_id();
    }
    for (auto&& link: implementation.links) {
        info.channels.emplace_back(make_channel(link, name, interface,
                                                implementation, info.channels, parent_links,
                                                parent_info.channels));
    }
    for (auto&& entry: implementation.nodes) {
        info.children.emplace(entry.first,
                              make_child(result, entry.first, entry.second,
                                         implementation.links, parent_ports));
    }
    return result;
}

auto make_child(instance& parent,
                const node_name& name,
                const node& node,
                const std::span<const link>& parent_links,
                const port_map& parent_ports) -> instance
{
    return std::visit(detail::overloaded{
        [&](const executable& implementation) {
            return make_child(name, node.interface, implementation,
                              parent_links, parent_ports);
        },
        [&](const system& implementation) {
            return make_child(parent, name, node.interface, implementation,
                              parent_links, parent_ports);
        }
    }, node.implementation);
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
        diags.flush();
        exit(exit_failure_code);
    }
}

auto set_found(const std::span<bool>& found, const port_id& port)
    -> void
{
    if (std::holds_alternative<reference_descriptor>(port)) {
        const auto d = int(std::get<reference_descriptor>(port));
        if ((d >= 0) && (unsigned(d) < size(found))) {
            found[d] = true;
        }
    }
}

auto set_found(const std::span<bool>& found, const std::set<port_id>& ports)
    -> void
{
    for (auto&& port: ports) {
        set_found(found, port);
    }
}

auto set_found(const std::span<bool>& found, const port_map& ports)
    -> void
{
    for (auto&& entry: ports) {
        set_found(found, entry.first);
    }
}

auto close_unused_ports(const node_name& name,
                        const std::span<const link>& conns,
                        const port_map& ports)
    -> void
{
    auto using_des = std::array<bool, 3u>{};
    for (auto&& conn: conns) {
        const auto ends = make_endpoints<node_endpoint>(conn);
        for (auto&& end: ends) {
            if (end && end->address == name) {
                set_found(using_des, end->ports);
            }
        }
    }
    set_found(using_des, ports);
    for (auto&& use: using_des) {
        if (!use) {
            ::close(int(&use - data(using_des)));
        }
    }
}

auto close_pipes_except(instance& root, instance& child) -> void
{
    if (const auto parent = find_parent(root, child)) {
        auto& parent_info = std::get<instance::system>(parent->info);
        auto& child_info = std::get<instance::forked>(child.info);
        for (auto&& pipe: the_pipe_registry().pipes) {
            if (!is_channel_for(parent_info.channels, pipe)) {
                pipe->close(pipe_channel::io::read, child_info.diags);
                pipe->close(pipe_channel::io::write, child_info.diags);
            }
        }
    }
}

auto setup(const node_name& name,
           const link& conn,
           channel& chan,
           std::ostream& diags) -> void
{
    const auto chan_p = fully_deref(&chan);
    assert(chan_p != nullptr);
    if (const auto pipe_p = std::get_if<pipe_channel>(chan_p)) {
        setup(name, conn, *pipe_p, diags);
        return;
    }
    if (const auto file_p = std::get_if<file_channel>(chan_p)) {
        setup(name, conn, *file_p, diags);
        return;
    }
    diags << "found UNKNOWN channel type!!!!\n";
}

auto setup(instance& root,
           const node_name& name,
           const port_map& ports,
           const std::span<const link>& links,
           const std::span<channel>& channels,
           instance& child) -> void
{
    // close & dup the channels needed for name, and close those that aren't
    auto& child_info = std::get<instance::forked>(child.info);
    const auto max_index = size(links);
    assert(max_index == size(channels));
    for (auto index = 0u; index < max_index; ++index) {
        setup(name, links[index], channels[index], child_info.diags);
    }
    close_unused_ports(name, links, ports);
    close_pipes_except(root, child);
}

auto find_file(const std::filesystem::path& file, const env_value& path)
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

auto fork_child(const node_name& name,
                const port_map& interface,
                const executable& implementation,
                const environment_map& env,
                instance& child,
                reference_process_id& pgrp,
                const std::span<const link>& links,
                const std::span<channel>& channels,
                instance& root,
                std::ostream& diags) -> void
{
    auto exe_path = implementation.file;
    if (exe_path.empty()) {
        diags << "no file specified to execute\n";
        return;
    }
    if (exe_path.is_relative() && !exe_path.has_parent_path()) {
        auto path_env_value = static_cast<const env_value*>(nullptr);
        if (const auto it = env.find("PATH"); it != env.end()) {
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
    auto arg_buffers = make_arg_bufs(implementation.arguments, exe_path);
    auto env_buffers = make_arg_bufs(env);
    auto argv = make_argv(arg_buffers);
    auto envp = make_argv(env_buffers);
    sigset_t old_set{};
    sigset_t new_set{};
    sigemptyset(&old_set);
    sigemptyset(&new_set);
    sigaddset(&new_set, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &new_set, &old_set);
    const auto pid = owning_process_id::fork();
    switch (pid) {
    case invalid_process_id:
        diags << "fork failed: " << os_error_code(errno) << "\n";
        return;
    case no_process_id: { // child process
        // Have to be careful here!
        // From https://man7.org/linux/man-pages/man2/fork.2.html:
        //   "in a multithreaded program, the child can safely call only
        //   async-signal-safe functions (see signal-safety(7)) until such
        //   time as it calls execve(2)."
        // See https://man7.org/linux/man-pages/man7/signal-safety.7.html
        // for "functions required to be async-signal-safe by POSIX.1".
        pthread_sigmask(SIG_SETMASK, &old_set, nullptr);
        if (::setpgid(0, int(pgrp)) == -1) {
            child_info.diags << "setpgid failed(0, ";
            child_info.diags << pgrp;
            child_info.diags << "): ";
            child_info.diags << os_error_code(errno);
            child_info.diags << "\n";
        }
        make_substitutions(argv);
        // Deal with:
        // link{
        //   node_endpoint{ls_process_name, flow::reference_descriptor{1}},
        //   node_endpoint{cat_process_name, flow::reference_descriptor{0}}
        // }
        //
        // Also:
        // Close file descriptors inherited by child that it's not using.
        // See: https://stackoverflow.com/a/7976880/7410358
        setup(root, name, interface, links, channels, child);
        // NOTE: child.diags streams opened close-on-exec, so no need
        //   to close them.
        if (!implementation.working_directory.empty()) {
            change_directory(implementation.working_directory, child_info.diags);
        }
        // NOTE: the following does not return!
        exec_child(exe_path, argv.data(), envp.data(), child_info.diags);
    }
    default: // case for the spawning/parent process
        child_info.state = owning_process_id(pid);
        if (pgrp == no_process_id) {
            pgrp = pid;
        }
        pthread_sigmask(SIG_SETMASK, &old_set, nullptr);
        break;
    }
}

auto fork_executables(const system& system,
                      instance& object,
                      instance& root,
                      std::ostream& diags) -> void
{
    auto& info = std::get<instance::system>(object.info);
    for (auto&& entry: system.nodes) {
        const auto& name = entry.first;
        const auto& node = entry.second;
        const auto found = info.children.find(name);
        if (found == info.children.end()) {
            diags << "can't find child instance for " << name << "!\n";
            continue;
        }
        std::visit(detail::overloaded{
            [&](const flow::executable& implementation) {
                fork_child(name, node.interface, implementation, system.environment,
                           found->second, info.pgrp, system.links, info.channels,
                           root, diags);
            },
            [&](const flow::system& implementation) {
                fork_executables(implementation, found->second, root, diags);
            }
        }, node.implementation);
    }
}

auto close_internal_ends(const link& link,
                         pipe_channel& channel,
                         std::ostream& diags) -> void
{
    static constexpr auto iowidth = 5;
    const auto ends = make_endpoints<node_endpoint>(link);
    if (ends[0] && (ends[0]->address != node_name{})) {
        const auto pio = pipe_channel::io::write;
        diags << "parent: closing ";
        diags << std::setw(iowidth) << pio << " side of ";
        diags << link << " " << channel << "\n";
        channel.close(pio, diags);
    }
    if (ends[1] && (ends[1]->address != node_name{})) {
        const auto pio = pipe_channel::io::read;
        diags << "parent: closing ";
        diags << std::setw(iowidth) << pio << " side of ";
        diags << link << " " << channel << "\n";
        channel.close(pio, diags);
    }
}

auto close_all_internal_ends(instance::system& instance,
                             const system& system,
                             std::ostream& diags) -> void
{
    const auto max_i = size(instance.channels);
    for (auto i = 0u; i < max_i; ++i) {
        auto& channel = instance.channels[i];
        const auto& link = system.links[i];
        if (const auto q = std::get_if<pipe_channel>(&channel)) {
            close_internal_ends(link, *q, diags);
            continue;
        }
    }
    for (auto&& entry: instance.children) {
        const auto& sub_name = entry.first;
        const auto custom = std::get_if<instance::system>(&(entry.second.info));
        if (custom) {
            const auto& sub_node = system.nodes.at(sub_name);
            close_all_internal_ends(*custom,
                                    std::get<flow::system>(sub_node.implementation),
                                    diags);
        }
    }
}

auto instantiate(const port_map& ports,
                 const executable& impl,
                 std::ostream& diags,
                 const instantiate_options& opts) -> instance
{
    instance result;
    if (!impl.file.has_filename()) {
        throw_has_no_filename(impl.file, "executable file path ");
    }
    const auto all_closed = confirm_closed({}, ports, {}, opts.ports);
    result.info = instance::forked{ext::temporary_fstream(), {}};
    auto pgrp = all_closed? no_process_id: current_process_id();
    fork_child({}, ports, impl, opts.environment, result, pgrp, {}, {},
               result, diags);
    return result;
}

auto instantiate(const port_map& ports,
                 const system& impl,
                 std::ostream& diags,
                 const instantiate_options& opts) -> instance
{
    instance result;
    using detail::make_channel;
    const auto all_closed = confirm_closed({}, ports, impl.links, opts.ports);
    result.info = instance::system{};
    auto& info = std::get<instance::system>(result.info);
    if (!all_closed) {
        info.pgrp = current_process_id();
    }
    {
        std::ostringstream os;
        auto prefix = "enclosing endpoint(s) not connected: ";
        for (auto&& entry: ports) {
            const auto look_for = node_endpoint{{}, entry.first};
            if (!find_index(impl.links, look_for)) {
                os << prefix << look_for;
                prefix = ", ";
            }
        }
        if (const auto msg = os.str(); !empty(msg)) {
            throw invalid_port_map{msg};
        }
    }
    info.channels.reserve(size(impl.links));
    for (auto&& link: impl.links) {
        info.channels.push_back(make_channel(link, {}, ports, impl,
                                             info.channels, {}, {}));
    }
    // Create all the subsystem instances before forking any!
    for (auto&& entry: impl.nodes) {
        const auto& sub_name = entry.first;
        const auto& sub_node = entry.second;
        info.children.emplace(sub_name,
                              make_child(result, sub_name, sub_node,
                                         impl.links, opts.ports));
    }
    fork_executables(impl, result, result, diags);
    // Only now, after making child processes,
    // close parent side of pipe_channels...
    close_all_internal_ends(info, impl, diags);
    return result;
}

}

auto instantiate(const node& node,
                 std::ostream& diags,
                 const instantiate_options& opts)
    -> instance
{
    return std::visit(detail::overloaded{
        [&](const executable& implementation) {
            return instantiate(node.interface, implementation, diags, opts);
        },
        [&](const system& implementation) {
            return instantiate(node.interface, implementation, diags, opts);
        }
    }, node.implementation);
}

}
