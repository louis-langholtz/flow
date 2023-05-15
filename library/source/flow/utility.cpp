#include <algorithm> // for std::find
#include <csignal> // for kill
#include <cstddef> // for std::ptrdiff_t
#include <cstdio> // for ::tmpfile, std::fclose
#include <cstdlib> // for ::mkstemp, ::fork
#include <functional> // for std::ref
#include <ios> // for std::boolalpha
#include <iterator> // for std::distance
#include <memory> // for std::unique_ptr
#include <streambuf>
#include <string> // for std::char_traits
#include <system_error> // for std::error_code

#include <fcntl.h> // for ::open
#include <sys/types.h> // for mkfifo
#include <sys/stat.h> // for mkfifo
#include <unistd.h> // for ::close

#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/instance.hpp"
#include "flow/os_error_code.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, ostream support
#include "flow/wait_result.hpp"
#include "flow/wait_status.hpp"

namespace flow {

namespace {

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
        os << "Diags are empty for " << name << "\n";
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

auto kill(const reference_process_id& pid, signal sig) -> int
{
    return ::kill(int(pid), to_posix_signal(sig));
}

}

auto nulldev_fstream() -> ext::fstream
{
    static constexpr auto dev_null_path = "/dev/null";
    constexpr auto mode =
        ext::filebuf::in|
        ext::filebuf::out;

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
    if (const auto p = std::get_if<instance::forked>(&object.info)) {
        if (!p->diags.is_open()) {
            os << "Diags are closed for " << name << "\n";
        }
        else {
            show_diags(os, name, p->diags);
        }
    }
    else if (const auto p = std::get_if<instance::custom>(&object.info)) {
        for (auto&& map_entry: p->children) {
            const auto full_name = name + map_entry.first;
            write_diags(system_name{full_name}, map_entry.second, os);
        }
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

auto is_matching(const unidirectional_connection& conn,
                 const system_endpoint& look_for) -> bool
{
    const auto ends = make_endpoints<system_endpoint>(conn);
    for (auto&& end: ends) {
        if (end) {
            if (end->address == look_for.address) {
                for (auto&& end_d: end->descriptors) {
                    for (auto&& look_d: look_for.descriptors) {
                        if (end_d == look_d) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

auto find_index(const std::span<const connection>& connections,
                const endpoint& look_for) -> std::optional<std::size_t>
{
    const auto first = std::begin(connections);
    const auto last = std::end(connections);
    const auto iter = std::find_if(first, last, [&look_for](const auto& c){
        const auto look_sys = std::get_if<system_endpoint>(&look_for);
        if (const auto p = std::get_if<unidirectional_connection>(&c)) {
            if (look_sys) {
                return is_matching(*p, *look_sys);
            }
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
    if (const auto p = std::get_if<instance::custom>(&instance.info)) {
        for (auto&& child: p->children) {
            send_signal(sig, name + child.first, child.second, diags);
        }
    }
    else if (const auto p = std::get_if<instance::forked>(&instance.info)) {
        if (const auto q = std::get_if<owning_process_id>(&(p->state))) {
            if ((reference_process_id(*q) != invalid_process_id) &&
                (reference_process_id(*q) != no_process_id)) {
                diags << "sending " << sig << " to " << name << "\n";
                if (kill(reference_process_id(*q), sig) == -1) {
                    diags << "kill(" << *q;
                    diags << "," << to_posix_signal(sig);
                    diags << ") failed: " << os_error_code(errno);
                    diags << "\n";
                }
            }
        }
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

auto get_matching_set(const descriptor_map& descriptors, io_type io)
    -> std::set<descriptor_id>
{
    auto result = std::set<descriptor_id>{};
    for (auto&& entry: descriptors) {
        if (entry.second.direction == io) {
            result.insert(entry.first);
        }
    }
    return result;
}

}
