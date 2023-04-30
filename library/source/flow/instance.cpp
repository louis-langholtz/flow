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
           const connection& c,
           pipe_channel& p,
           std::ostream& diags) -> void
{
    const auto ports = std::array<const prototype_port*, 2u>{
        std::get_if<prototype_port>(&c.ports[0]),
        std::get_if<prototype_port>(&c.ports[1])
    };
    if (!ports[0] || !ports[1]) {
        return;
    }
    if ((ports[0]->address != name) && (ports[1]->address != name)) {
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
    if (ports[0]->address == name) {
        diags << name << " " << c << " " << p;
        diags << ", close read-side\n";
        if (!p.close(pipe_channel::io::read, diags)) { // close unused end
            ::_exit(1);
        }
        diags << name << " " << c << " " << p;
        diags << ", dup write-side to " << ports[0]->descriptor << "\n";
        if (!p.dup(pipe_channel::io::write, ports[0]->descriptor, diags)) {
            ::_exit(1);
        }
    }
    if (ports[1]->address == name) {
        diags << name << " " << c << " " << p;
        diags << ", close write-side\n";
        if (!p.close(pipe_channel::io::write, diags)) { // close unused end
            ::_exit(1);
        }
        diags << name << " " << c << " " << p;
        diags << ", dup read-side to " << ports[1]->descriptor << "\n";
        if (!p.dup(pipe_channel::io::read, ports[1]->descriptor, diags)) {
            ::_exit(1);
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

auto setup(const prototype_name& name,
           const connection& c,
           file_channel& p,
           std::ostream& diags) -> void
{
    diags << name << " " << c << ", open & dup2\n";
    const auto file_ports = std::array<const file_port *, 2u>{
        std::get_if<file_port>(&c.ports[0]),
        std::get_if<file_port>(&c.ports[1])
    };
    const auto& file_port = file_ports[0]? file_ports[0]: file_ports[1];
    const auto& other_port = file_ports[0]? c.ports[1]: c.ports[0];
    if (const auto op = std::get_if<prototype_port>(&other_port)) {
        const auto flags = to_open_flags(p.io);
        const auto mode = 0600;
        const auto fd = ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
                               file_port->path.c_str(), flags, mode);
        if (fd == -1) {
            static constexpr auto mode_width = 5;
            diags << "open file " << file_port->path << " with mode ";
            diags << std::oct << std::setfill('0') << std::setw(mode_width) << flags;
            diags << " failed: " << system_error_code(errno) << "\n";
            ::_exit(1);
        }
        if (::dup2(fd, int(op->descriptor)) == -1) {
            diags << "dup2(" << fd << "," << op->descriptor << ") failed: ";
            diags << system_error_code(errno) << "\n";
            ::_exit(1);
        }
    }
}

auto setup(const prototype_name& name,
           const connection& c,
           channel& channel,
           std::ostream& diags) -> void
{
    if (const auto p = std::get_if<pipe_channel>(&channel)) {
        return setup(name, c, *p, diags);
    }
    if (const auto p = std::get_if<file_channel>(&channel)) {
        return setup(name, c, *p, diags);
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
    std::vector<std::array<io_type, 2u>> io_types;
    for (auto&& connection: system.connections) {
        auto ports_io = std::array<io_type, 2u>{};
        auto have_file_port = false;
        for (auto&& port: connection.ports) {
            const auto i = &port - connection.ports.data();
            if (const auto p = std::get_if<prototype_port>(&port)) {
                if (p->address == prototype_name{}) {
                    const auto& d_info = system.descriptors.at(p->descriptor);
                    ports_io[i] = reverse(d_info.direction);
                    continue;
                }
                const auto& child = system.prototypes.at(p->address);
                if (const auto cp = std::get_if<system_prototype>(&child)) {
                    const auto& d_info = cp->descriptors.at(p->descriptor);
                    ports_io[i] = d_info.direction;
                    continue;
                }
                if (const auto cp = std::get_if<executable_prototype>(&child)) {
                    const auto& d_info = cp->descriptors.at(p->descriptor);
                    ports_io[i] = d_info.direction;
                    continue;
                }
                throw std::logic_error{"unknown proto type"};
            }
            if (const auto p = std::get_if<file_port>(&port)) {
                if (have_file_port) {
                    throw std::invalid_argument{"cant't connect file to file"};
                }
                have_file_port = true;
                ports_io[i] = io_type::bidir;
                continue;
            }
            throw std::logic_error{"unknown port type"};
        }
        if (ports_io[0] == ports_io[1]) {
            throw std::invalid_argument{"bad connection: same io unsupported"};
        }
        io_types.push_back(ports_io);
        if (have_file_port) {
            const auto io = (ports_io[0] != io_type::bidir)
                ? ports_io[0]: ports_io[1];
            channels.push_back(file_channel{io});
        }
        else {
            channels.push_back(pipe_channel{});
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
    // Only now, **after fork calls**, close parent side pipe_channels...
    for (auto&& channel: channels) {
        const auto i = &channel - channels.data();
        const auto& io_pair = io_types[i];
        const auto& connection = system.connections[i];
        if (const auto p = std::get_if<pipe_channel>(&channel)) {
            const auto ports = std::array<const prototype_port*, 2u>{
                std::get_if<prototype_port>(&connection.ports[0]),
                std::get_if<prototype_port>(&connection.ports[1])
            };
            const auto a_to_b = (io_pair[0] == io_type::out)
                             || (io_pair[1] == io_type::in);
            if (ports[0]->address != prototype_name{}) {
                const auto pio = a_to_b? pipe_channel::io::write: pipe_channel::io::read;
                diags << "parent: closing " << pio << " side of ";
                diags << connection << " " << *p << "\n";
                p->close(pio, diags);
            }
            if (ports[1]->address != prototype_name{}) {
                const auto pio = a_to_b? pipe_channel::io::read: pipe_channel::io::write;
                diags << "parent: closing " << pio << " side of ";
                diags << connection << " " << *p << "\n";
                p->close(pio, diags);
            }
        }
    }
    result.channels = std::move(channels);
    return result;
}

}
