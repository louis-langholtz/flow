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
#include "wait_result.hpp"
#include "wait_status.hpp"

#include "flow/descriptor_id.hpp"
#include "flow/instance.hpp"
#include "flow/prototype.hpp"
#include "flow/utility.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, plus ostream support

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

auto wait_for_child() -> wait_result
{
    auto status = 0;
    auto pid = decltype(::wait(&status)){};
    do { // NOLINT(cppcoreguidelines-avoid-do-while)
        pid = ::wait(&status);
    } while ((pid == -1) && (errno == EINTR));
    if (pid == -1 && errno == ECHILD) {
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
        switch (wait_result::info_t::status_enum(result.info().status.index())) {
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

auto temporary_fstream() -> ext::fstream
{
    // "w+xb"
    constexpr auto mode =
        ext::fstream::in|
        ext::fstream::out|
        ext::fstream::trunc|
        ext::fstream::binary|
        ext::fstream::noreplace|
        ext::fstream::tmpfile;

    ext::fstream stream;
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
