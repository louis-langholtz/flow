#include <algorithm> // for std::find
#include <cstddef> // for std::ptrdiff_t
#include <cstdio> // for ::tmpfile, std::fclose
#include <ios> // for std::boolalpha
#include <iterator> // for std::distance
#include <memory> // for std::unique_ptr
#include <streambuf>
#include <string> // for std::char_traits
#include <system_error> // for std::error_code

#include <fcntl.h> // for ::open
#include <stdlib.h> // for ::mkstemp, ::fork
#include <unistd.h> // for ::close
#include <sys/wait.h>
#include <sys/types.h> // for mkfifo
#include <sys/stat.h> // for mkfifo

#include "system_error_code.hpp"
#include "wait_status.hpp"

#include "flow/descriptor_id.hpp"
#include "flow/expected.hpp"
#include "flow/instance.hpp"
#include "flow/prototype.hpp"
#include "flow/utility.hpp"

namespace flow {

namespace {

auto find(std::map<prototype_name, instance>& instances,
          process_id pid) -> std::map<prototype_name, instance>::value_type*
{
    const auto it = std::find_if(std::begin(instances),
                                 std::end(instances),
                                 [pid](const auto& e) {
        return e.second.id == pid;
    });
    return (it != std::end(instances)) ? &(*it): nullptr;
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

struct wait_result {
    enum type_enum: std::size_t {no_children = 0u, has_error, has_info};

    using error_t = system_error_code;

    struct info_t {
        enum status_enum: std::size_t {unknown, exit, signaled, stopped, continued};
        process_id id;
        wait_status status;
    };

    wait_result() noexcept = default;

    wait_result(std::monostate) noexcept: wait_result{} {};

    wait_result(error_t error): value{error} {}

    wait_result(info_t info): value{info} {}

    constexpr auto type() const noexcept -> type_enum
    {
        return static_cast<type_enum>(value.index());
    }

    constexpr explicit operator bool() const noexcept
    {
        return type() != no_children;
    }

    auto error() const& -> const error_t&
    {
        return std::get<error_t>(value);
    }

    auto info() const& -> const info_t&
    {
        return std::get<info_t>(value);
    }

private:
    std::variant<std::monostate, error_t, info_t> value;
};

auto operator<<(std::ostream& os, const wait_status& status) -> std::ostream&
{
    switch (wait_result::info_t::status_enum(status.index())) {
    case wait_result::info_t::unknown:
        os << "unknown-wait-status-type";
        break;
    case wait_result::info_t::exit:
        os << std::get<wait_exit_status>(status);
        break;
    case wait_result::info_t::signaled:
        os << std::get<wait_signaled_status>(status);
        break;
    case wait_result::info_t::stopped:
        os << std::get<wait_stopped_status>(status);
        break;
    case wait_result::info_t::continued:
        os << std::get<wait_continued_status>(status);
        break;
    }
    return os;
}

auto wait_for_child() -> wait_result
{
    auto status = 0;
    auto pid = decltype(::wait(&status)){};
    do { // NOLINT(cppcoreguidelines-avoid-do-while)
        pid = ::wait(&status);
    } while ((pid == -1) && (errno == EINTR));
    if (pid == -1 && errno == ECHILD) {
        return std::monostate{};
    }
    if (pid == -1) {
        return system_error_code(errno);
    }
    if (WIFEXITED(status)) {
        return wait_result::info_t{process_id{pid},
            wait_exit_status{WEXITSTATUS(status)}};
    }
    if (WIFSIGNALED(status)) {
        return wait_result::info_t{process_id{pid},
            wait_signaled_status{WTERMSIG(status), WCOREDUMP(status) != 0}};
    }
    if (WIFSTOPPED(status)) {
        return wait_result::info_t{process_id{pid},
            wait_stopped_status{WSTOPSIG(status)}};
    }
    if (WIFCONTINUED(status)) {
        return wait_result::info_t{process_id{pid},
            wait_continued_status{}};
    }
    return wait_result::info_t{process_id{pid}};
}

auto handle(instance& instance, const wait_result& result,
            std::ostream& err_stream, wait_diags diags) -> void
{
    switch (result.type()) {
    case wait_result::no_children:
        break;
    case wait_result::has_error:
        err_stream << "wait failed: " << result.error() << "\n";
        break;
    case wait_result::has_info: {
        const auto entry = find(instance.children, result.info().id);
        switch (result.info().status.index()) {
        case wait_result::info_t::unknown:
            break;
        case wait_result::info_t::exit: {
            const auto exit_status = std::get<wait_exit_status>(result.info().status);
            if (entry) {
                entry->second.id = process_id(0);
            }
            if ((diags == wait_diags::yes) || (exit_status.value != 0)) {
                err_stream << "child=" << (entry? entry->first.value: "unknown") << ", ";
                err_stream << "pid=" << result.info().id;
                err_stream << ", " << exit_status;
                err_stream << "\n";
            }
            break;
        }
        case wait_result::info_t::signaled: {
            const auto signaled_status = std::get<wait_signaled_status>(result.info().status);
            err_stream << "child=" << (entry? entry->first.value: "unknown") << ", ";
            err_stream << signaled_status;
            break;
        }
        case wait_result::info_t::stopped: {
            const auto stopped_status = std::get<wait_stopped_status>(result.info().status);
            err_stream << "child=" << (entry? entry->first.value: "unknown") << ", ";
            err_stream << stopped_status;
            break;
        }
        case wait_result::info_t::continued: {
            const auto continued_status = std::get<wait_continued_status>(result.info().status);
            err_stream << "child=" << (entry? entry->first.value: "unknown") << ", ";
            err_stream << continued_status;
            break;
        }
        }
        break;
    }
    }
}

auto show_diags(std::ostream& os, const prototype_name& name,
                std::iostream& diags) -> void
{
    if (diags.rdstate()) {
        os << "diags stream not good for " << name << "\n";
        return;
    }
    diags.seekg(0, std::ios_base::end);
    if (diags.rdstate()) {
        os << "diags stream not good for " << name << " after seekg\n";
        return;
    }
    const auto endpos = diags.tellg();
    switch (endpos) {
    case -1:
        os << "unable to tell where diags end is for " << name << "\n";
        return;
    case 0:
        os << "Diags are empty for '" << name << "'\n";
        return;
    default:
        break;
    }
    diags.seekg(0, std::ios_base::beg);
    os << "Diagnostics for " << name << " having " << endpos << "b...\n";
    // istream_iterator skips ws, so use istreambuf_iterator...
    std::copy(std::istreambuf_iterator<char>(diags.rdbuf()),
              std::istreambuf_iterator<char>(),
              std::ostream_iterator<char>(os));
}

}

auto temporary_fstream() -> fstream
{
    // "w+xb"
    constexpr auto mode =
        fstream::in|fstream::out|fstream::trunc|fstream::binary|fstream::noreplace|fstream::tmpfile;

    fstream stream;
    stream.open(std::filesystem::temp_directory_path(), mode);
    return stream;
}

auto make_arg_bufs(const std::vector<std::string>& strings,
                   const std::string& fallback)
-> std::vector<std::string>
{
    auto result = std::vector<std::string>{};
    if (strings.empty()) {
        if (!fallback.empty()) {
            result.push_back(fallback);
        }
    }
    else {
        for (auto&& string: strings) {
            result.push_back(string);
        }
    }
    return result;
}

auto make_arg_bufs(const std::map<std::string, std::string>& envars)
-> std::vector<std::string>
{
    auto result = std::vector<std::string>{};
    for (const auto& envar: envars) {
        result.push_back(envar.first + "=" + envar.second);
    }
    return result;
}

auto make_argv(const std::span<std::string>& args)
-> std::vector<char*>
{
    auto result = std::vector<char*>{};
    for (auto&& arg: args) {
        result.push_back(arg.data());
    }
    result.push_back(nullptr); // last element must always be nullptr!
    return result;
}

auto write(std::ostream& os, const std::error_code& ec)
    -> std::ostream&
{
    os << ec << " (" << ec.message() << ")";
    return os;
}

auto write_diags(const prototype_name& name, instance& object,
                std::ostream& os) -> void
{
    if (!object.diags.is_open()) {
        os << "Diags are closed for '" << name << "'\n";
    }
    else {
        show_diags(os, name, object.diags);
    }
    for (auto&& map_entry: object.children) {
        const auto full_name = name.value + "." + map_entry.first.value;
        write_diags(prototype_name{full_name}, map_entry.second, os);
    }
}

auto find_channel_index(const std::span<const connection>& connections,
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

auto touch(const file_port& file) -> void
{
    static constexpr auto flags = O_CREAT|O_WRONLY;
    static constexpr auto mode = 0666;
    if (const auto fd = ::open(file.path.c_str(), flags, mode); fd != -1) { // NOLINT(cppcoreguidelines-pro-type-vararg)
        ::close(fd);
        return;
    }
    throw std::runtime_error{to_string(system_error_code(errno))};
}

auto mkfifo(const file_port& file) -> void
{
    static constexpr auto fifo_mode = ::mode_t{0666};
    if (::mkfifo(file.path.c_str(), fifo_mode) == -1) {
        throw std::runtime_error{to_string(system_error_code(errno))};
    }
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
            case process_id{0}: // child process
                // NOTE: the following does not return!
                instantiate(name, exe_proto, argv.data(), envp.data(),
                            system.connections, channels, errs);
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

auto wait(instance& instance, std::ostream& err_stream, wait_diags diags) -> void
{
    if (diags == wait_diags::yes) {
        err_stream << "wait called for " << std::size(instance.children) << " children\n";
    }
    auto result = decltype(wait_for_child()){};
    while (bool(result = wait_for_child())) {
        handle(instance, result, err_stream, diags);
    }
}

}
