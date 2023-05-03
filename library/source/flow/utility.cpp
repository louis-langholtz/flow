#include <algorithm> // for std::find
#include <cstddef> // for std::ptrdiff_t
#include <cstdio> // for ::tmpfile, std::fclose
#include <functional> // for std::ref
#include <ios> // for std::boolalpha
#include <iterator> // for std::distance
#include <memory> // for std::unique_ptr
#include <streambuf>
#include <string> // for std::char_traits
#include <system_error> // for std::error_code

#include <fcntl.h> // for ::open
#include <signal.h> // for kill
#include <stdlib.h> // for ::mkstemp, ::fork
#include <sys/wait.h>
#include <sys/time.h> // for setitimer
#include <sys/types.h> // for mkfifo
#include <sys/stat.h> // for mkfifo
#include <unistd.h> // for ::close

#include "os_error_code.hpp"
#include "wait_result.hpp"
#include "wait_status.hpp"

#include "flow/descriptor_id.hpp"
#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, ostream support

namespace flow {

namespace {

auto find(const system_name& name, instance& object, process_id pid)
    -> std::optional<decltype(std::make_pair(name, std::ref(object)))>
{
    if (object.pid == pid) {
        return std::make_pair(name, std::ref(object));
    }
    for (auto&& entry: object.children) {
        if (auto found = find(name + entry.first, entry.second, pid)) {
            return found;
        }
    }
    return {};
}

auto wait_for_child() -> wait_result
{
    auto status = 0;
    auto pid = decltype(::wait(&status)){};
    auto err = 0;
    do { // NOLINT(cppcoreguidelines-avoid-do-while)
        //itimerval new_timer{};
        //itimerval old_timer;
        //setitimer(ITIMER_REAL, &new_timer, &old_timer);
        pid = ::wait(&status);
        err = errno;
        //setitimer(ITIMER_REAL, &old_timer, nullptr);
    } while ((pid == -1) && (err == EINTR));
    if ((pid == -1) && (err == ECHILD)) {
        return wait_result::no_kids_t{};
    }
    if (pid == -1) {
        return wait_result::error_t(errno);
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

auto handle(const system_name& name, instance& instance,
            const wait_result& result, std::ostream& diags,
            wait_mode mode) -> void
{
    switch (result.type()) {
    case wait_result::no_children:
        break;
    case wait_result::has_error:
        diags << "wait failed: " << result.error() << "\n";
        break;
    case wait_result::has_info: {
        static const auto unknown_name = system_name{"unknown"};
        const auto entry = find(name, instance, result.info().id);
        using status_enum = wait_result::info_t::status_enum;
        switch (status_enum(result.info().status.index())) {
        case wait_result::info_t::unknown:
            break;
        case wait_result::info_t::exit: {
            const auto exit_status = std::get<wait_exit_status>(result.info().status);
            if (entry) {
                entry->second.pid = process_id(0);
            }
            if ((mode == wait_mode::diagnostic) || (exit_status.value != 0)) {
                diags << "child=" << (entry? entry->first: unknown_name);
                diags << ", pid=" << result.info().id;
                diags << ", " << exit_status;
                diags << "\n";
            }
            break;
        }
        case wait_result::info_t::signaled: {
            const auto signaled_status = std::get<wait_signaled_status>(result.info().status);
            diags << "child=" << (entry? entry->first: unknown_name);
            diags << ", pid=" << result.info().id;
            diags << ", " << signaled_status;
            diags << "\n";
            break;
        }
        case wait_result::info_t::stopped: {
            const auto stopped_status = std::get<wait_stopped_status>(result.info().status);
            diags << "child=" << (entry? entry->first: unknown_name);
            diags << ", pid=" << result.info().id;
            diags << ", " << stopped_status;
            diags << "\n";
            break;
        }
        case wait_result::info_t::continued: {
            const auto continued_status = std::get<wait_continued_status>(result.info().status);
            diags << "child=" << (entry? entry->first: unknown_name);
            diags << ", pid=" << result.info().id;
            diags << ", " << continued_status;
            diags << "\n";
            break;
        }
        }
        break;
    }
    }
}

auto show_diags(std::ostream& os, const system_name& name,
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

auto to_posix_signal(signal sig) -> int
{
    switch (sig) {
    case signal::interrupt:
        return SIGINT;
    case signal::terminate:
        return SIGTERM;
    case signal::kill:
        return SIGKILL;
    }
    throw std::invalid_argument{"unknown signal"};
}

auto sigaction_cb(int sig, siginfo_t *info, void * /*ucontext*/) -> void
{
    const auto sender_pid = info? info->si_pid: -1;
    std::cerr << "caught " << sig << ", from " << sender_pid << "\n";
}

}

auto temporary_fstream() -> ext::fstream
{
    // "w+xb"
    constexpr auto mode =
        ext::fstream::in|
        ext::fstream::out|
        ext::fstream::trunc|
        ext::fstream::binary|
        ext::fstream::noreplace|
        ext::fstream::tmpfile|
        ext::fstream::cloexec;

    ext::fstream stream;
    stream.open(std::filesystem::temp_directory_path(), mode);
    return stream;
}

auto nulldev_fstream() -> ext::fstream
{
    static constexpr auto dev_null_path = "/dev/null";
    constexpr auto mode =
        ext::fstream::in|
        ext::fstream::out;

    ext::fstream stream;
    stream.open(dev_null_path, mode);
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

auto write_diags(const system_name& name, instance& object,
                std::ostream& os) -> void
{
    if (!object.diags.is_open()) {
        os << "Diags are closed for '" << name << "'\n";
    }
    else {
        show_diags(os, name, object.diags);
    }
    for (auto&& map_entry: object.children) {
        const auto full_name = name + map_entry.first;
        write_diags(system_name{full_name}, map_entry.second, os);
    }
}

auto find_index(const std::span<const connection>& connections,
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

auto find_index(const std::span<const connection>& connections,
                const endpoint& look_for) -> std::optional<std::size_t>
{
    const auto first = std::begin(connections);
    const auto last = std::end(connections);
    const auto iter = std::find_if(first, last, [&look_for](const auto& c){
        if (const auto p = std::get_if<unidirectional_connection>(&c)) {
            return (p->src == look_for) || (p->dst == look_for);
        }
        if (const auto p = std::get_if<bidirectional_connection>(&c)) {
            return (p->ends[0] == look_for) || (p->ends[1] == look_for);
        }
        return false;
    });
    if (iter != last) {
        return {std::distance(first, iter)};
    }
    return {};
}

auto touch(const file_endpoint& file) -> void
{
    static constexpr auto flags = O_CREAT|O_WRONLY;
    static constexpr auto mode = 0666;
    if (const auto fd = ::open(file.path.c_str(), flags, mode); fd != -1) { // NOLINT(cppcoreguidelines-pro-type-vararg)
        ::close(fd);
        return;
    }
    throw std::runtime_error{to_string(os_error_code(errno))};
}

auto mkfifo(const file_endpoint& file) -> void
{
    static constexpr auto fifo_mode = ::mode_t{0666};
    if (::mkfifo(file.path.c_str(), fifo_mode) == -1) {
        throw std::runtime_error{to_string(os_error_code(errno))};
    }
}

auto wait(const system_name& name, instance& instance,
          std::ostream& diags, wait_mode mode)
    -> void
{
    if (mode == wait_mode::diagnostic) {
        diags << "wait called for instance with total of ";
        diags << total_descendants(instance) << " descendants, ";
        diags << total_channels(instance) << " channels.\n";
    }
    auto result = decltype(wait_for_child()){};
    while (bool(result = wait_for_child())) {
        handle(name, instance, result, diags, mode);
    }
    if (mode == wait_mode::diagnostic) {
        diags << "wait finished.\n";
    }
}

auto operator<<(std::ostream& os, signal s) -> std::ostream&
{
    switch (s) {
    case signal::interrupt:
        os << "sigint";
        break;
    case signal::terminate:
        os << "sigterm";
        break;
    case signal::kill:
        os << "sigkill";
        break;
    }
    return os;
}

auto send_signal(signal sig,
                 const system_name& name,
                 const instance& instance,
                 std::ostream& diags) -> void
{
    for (auto&& child: instance.children) {
        send_signal(sig, name + child.first, child.second, diags);
    }
    if ((instance.pid != invalid_process_id) &&
        (instance.pid != no_process_id)) {
        diags << "sending " << sig << " to " << name << "\n";
        if (::kill(int(instance.pid), to_posix_signal(sig)) == -1) {
            diags << "kill(" << instance.pid;
            diags << "," << to_posix_signal(sig);
            diags << ") failed: " << os_error_code(errno);
            diags << "\n";
        }
        return;
    }
}

auto set_signal_handler(signal sig) -> void
{
    struct sigaction act{};
    act.sa_sigaction = sigaction_cb;
    act.sa_flags = SA_SIGINFO;
    if (::sigaction(to_posix_signal(sig), &act, nullptr) == -1) {
        throw std::system_error{errno, std::system_category()};
    }
}

}
