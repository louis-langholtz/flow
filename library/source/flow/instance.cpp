#include <cstring> // for std::strcmp

#include <unistd.h> // for getpid, setpgid
#include <fcntl.h> // for ::open

#include "indenting_ostreambuf.hpp"
#include "system_error_code.hpp"

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
auto make_endpoints(const connection& c) -> std::array<const T*, 2u>
{
    return {
        std::get_if<T>(&c.ends[0]), // NOLINT(readability-container-data-pointer)
        std::get_if<T>(&c.ends[1])
    };
}

auto setup(const system_name& name,
           const connection& c,
           pipe_channel& p,
           std::ostream& diags) -> void
{
    const auto ends = make_endpoints<system_endpoint>(c);
    if (!ends[0] || !ends[1]) {
        return;
    }
    if ((ends[0]->address != name) && (ends[1]->address != name)) {
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
    if (ends[0]->address == name) {
        diags << name << " " << c << " " << p;
        diags << ", close read-side\n";
        if (!p.close(pipe_channel::io::read, diags)) { // close unused end
            ::_exit(1);
        }
        diags << name << " " << c << " " << p;
        diags << ", dup write-side to " << ends[0]->descriptor << "\n";
        if (!p.dup(pipe_channel::io::write, ends[0]->descriptor, diags)) {
            ::_exit(1);
        }
    }
    if (ends[1]->address == name) {
        diags << name << " " << c << " " << p;
        diags << ", close write-side\n";
        if (!p.close(pipe_channel::io::write, diags)) { // close unused end
            ::_exit(1);
        }
        diags << name << " " << c << " " << p;
        diags << ", dup read-side to " << ends[1]->descriptor << "\n";
        if (!p.dup(pipe_channel::io::read, ends[1]->descriptor, diags)) {
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

auto setup(const system_name& name,
           const connection& c,
           file_channel& p,
           std::ostream& diags) -> void
{
    diags << name << " " << c << ", open & dup2\n";
    const auto file_ends = make_endpoints<file_endpoint>(c);
    const auto& file_end = file_ends[0]? file_ends[0]: file_ends[1];
    const auto& other_end = file_ends[0]? c.ends[1]: c.ends[0];
    if (const auto op = std::get_if<system_endpoint>(&other_end)) {
        const auto flags = to_open_flags(p.io);
        const auto mode = 0600;
        const auto fd = ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
                               file_end->path.c_str(), flags, mode);
        if (fd == -1) {
            static constexpr auto mode_width = 5;
            diags << "open file " << file_end->path << " with mode ";
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
auto instantiate(const system_name& name,
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
    const auto ec = system_error_code(errno);
    diags << "execve of " << exe_proto.executable_file << "failed: " << ec << "\n";
    diags.flush();
    ::_exit(1);
}

auto instantiate(const system_name& name,
                 const executable_system& exe_proto,
                 process_id pgrp,
                 const std::span<const connection>& connections,
                 const std::span<channel>& channels,
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
        diags << "child '" << name << "' started as pid " << pid << "\n";
        break;
    }
    return instance{pid, std::move(child_diags)};
}

using connection_io_types = std::array<io_type, 2u>;
using connection_result = std::pair<connection_io_types, channel>;

auto make_channel(const system_name& name, const custom_system& system,
                  const connection& conn,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> connection_result
{
    static const auto unequal_sizes_error =
        "size of parent connections not equal size of parent channels";
    static const auto no_file_file_error =
        "cant't connect file to file";
    static const auto same_iotypes_error =
        "bad connection: same io unsupported";
    static const auto same_endpoints_error =
        "connection must have different endpoints";
    // static const auto not_closed_error = "system must be closed";

    if (conn.ends[0] == conn.ends[1]) {
        throw std::invalid_argument{same_endpoints_error};
    }
    if (std::size(parent_connections) != std::size(parent_channels)) {
        throw std::invalid_argument{unequal_sizes_error};
    }

    auto ends_io = connection_io_types{};
    auto have_file_endpoint = false;
    auto enclosure_descriptors = std::array<descriptor_id, 2u>{
        invalid_descriptor_id, invalid_descriptor_id
    };
    for (auto&& end: conn.ends) {
        const auto i = &end - conn.ends.data();
        if (const auto p = std::get_if<system_endpoint>(&end)) {
            if (p->address == system_name{}) {
                const auto& d_info = system.descriptors.at(p->descriptor);
                enclosure_descriptors[i] = p->descriptor;
                ends_io[i] = reverse(d_info.direction);
                continue;
            }
            const auto& child = system.prototypes.at(p->address);
            if (const auto cp = std::get_if<custom_system>(&child)) {
                const auto& d_info = cp->descriptors.at(p->descriptor);
                ends_io[i] = d_info.direction;
                continue;
            }
            if (const auto cp = std::get_if<executable_system>(&child)) {
                const auto& d_info = cp->descriptors.at(p->descriptor);
                ends_io[i] = d_info.direction;
                continue;
            }
            throw std::logic_error{"unknown proto type"};
        }
        if (const auto p = std::get_if<file_endpoint>(&end)) {
            if (have_file_endpoint) {
                throw std::invalid_argument{no_file_file_error};
            }
            have_file_endpoint = true;
            ends_io[i] = io_type::bidir;
            continue;
        }
        throw std::logic_error{"unknown endpoint type"};
    }
    if (ends_io[0] == ends_io[1]) {
        throw std::invalid_argument{same_iotypes_error};
    }
    if (have_file_endpoint) {
        const auto io = (ends_io[0] != io_type::bidir)
            ? ends_io[0]: ends_io[1];
        return {ends_io, file_channel{io}};
    }
    for (auto&& descriptor_id: enclosure_descriptors) {
        if (descriptor_id != invalid_descriptor_id) {
            const auto look_for = system_endpoint{name, descriptor_id};
            if (const auto found = find_index(parent_connections, look_for)) {
                return {ends_io, reference_channel{&parent_channels[*found]}};
            }
            // throw std::invalid_argument{not_closed_error};
        }
    }
    return {ends_io, pipe_channel{}};
}

auto make_child(const system_name& name,
                const system& proto,
                process_id& pgrp,
                const std::span<const connection>& connections,
                const std::span<channel>& channels,
                std::ostream& diags) -> instance
{
    if (const auto p = std::get_if<executable_system>(&proto)) {
        auto kid = instantiate(name, *p, process_id(-int(pgrp)),
                               connections, channels, diags);
        if (kid.id != invalid_process_id && pgrp == no_process_id) {
            pgrp = process_id(-int(kid.id));
        }
        return kid;
    }
    if (const auto p = std::get_if<custom_system>(&proto)) {
        return instantiate(name, *p, diags, connections, channels);
    }
    diags << "parent: unrecognized prototype for " << name << "\n";
    return {};
}

}

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&
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

auto pretty_print(std::ostream& os, const instance& value) -> void
{
    os << "{\n";
    os << "  .id=" << value.id << ",\n";
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
            os << "    " << channel << ",\n";
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
    result.id = no_process_id;
    std::vector<channel> channels;
    std::vector<connection_io_types> io_types;
    for (auto&& connection: system.connections) {
        auto connection_result = make_channel(name, system, connection,
                                              parent_connections,
                                              parent_channels);
        io_types.push_back(connection_result.first);
        channels.push_back(std::move(connection_result.second));
    }
    for (auto&& mapentry: system.prototypes) {
        const auto& child = mapentry.first;
        const auto& proto = mapentry.second;
        auto kid = make_child(child, proto, result.id,
                              system.connections, channels, diags);
        result.children.emplace(child, std::move(kid));
    }
    // Only now, after making child processes,
    // close parent side of pipe_channels...
    for (auto&& channel: channels) {
        const auto i = &channel - channels.data();
        const auto& io_pair = io_types[i];
        const auto& connection = system.connections[i];
        if (const auto p = std::get_if<pipe_channel>(&channel)) {
            const auto ends = make_endpoints<system_endpoint>(connection);
            const auto a_to_b = (io_pair[0] == io_type::out)
                             || (io_pair[1] == io_type::in);
            if (ends[0]->address != system_name{}) {
                const auto pio = a_to_b? pipe_channel::io::write: pipe_channel::io::read;
                diags << "parent '" << name << "': closing " << pio << " side of ";
                diags << connection << " " << *p << "\n";
                p->close(pio, diags);
            }
            if (ends[1]->address != system_name{}) {
                const auto pio = a_to_b? pipe_channel::io::read: pipe_channel::io::write;
                diags << "parent '" << name << "': closing " << pio << " side of ";
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
    result.channels = std::move(channels);
    return result;
}

}
