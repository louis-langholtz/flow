#include <sys/wait.h>

#include "flow/instance.hpp"
#include "flow/system_name.hpp"
#include "flow/utility.hpp"
#include "flow/wait_result.hpp"

namespace flow {

namespace {

auto find(const system_name& name, instance& object,
          const reference_process_id& pid)
    -> std::optional<decltype(std::make_pair(name, std::ref(object)))>
{
    if (const auto p = std::get_if<instance::forked>(&object.info)) {
        if (const auto q = std::get_if<owning_process_id>(&p->state)) {
            if (reference_process_id(*q) == pid) {
                return std::make_pair(name, std::ref(object));
            }
        }
        return {};
    }
    if (const auto p = std::get_if<instance::custom>(&object.info)) {
        for (auto&& entry: p->children) {
            if (auto found = find(name + entry.first, entry.second, pid)) {
                return found;
            }
        }
    }
    return {};
}

auto handle(const system_name& name, instance& instance,
            const wait_result::info_t& info) -> bool
{
    static const auto unknown_name = system_name{"unknown"};
    const auto entry = find(name, instance, info.id);
    using status_enum = wait_result::info_t::status_enum;
    switch (status_enum(info.status.index())) {
    case wait_result::info_t::unknown:
        break;
    case wait_result::info_t::exit: {
        const auto status = std::get<wait_exit_status>(info.status);
        if (entry) {
            (std::get<instance::forked>(entry->second.info)).state =
                wait_status(status);
        }
        break;
    }
    case wait_result::info_t::signaled: {
        const auto status = std::get<wait_signaled_status>(info.status);
        if (entry) {
            (std::get<instance::forked>(entry->second.info)).state =
                wait_status(status);
        }
        break;
    }
    case wait_result::info_t::stopped: {
        return false;
    }
    case wait_result::info_t::continued: {
        return false;
    }
    }
    return true;
}

auto handle(const system_name& name, instance& instance,
            const wait_result& result) -> bool
{
    switch (result.type()) {
    case wait_result::no_children:
        return false;
    case wait_result::has_error:
        return true;
    case wait_result::has_info:
        return handle(name, instance, result.info());
    }
    return true;
}

}

auto operator<<(std::ostream& os, const wait_result::no_kids_t&)
    -> std::ostream&
{
    os << "no child processes to wait for";
    return os;
}

auto operator<<(std::ostream& os, const wait_result::info_t& value)
    -> std::ostream&
{
    os << value.id << ", " << value.status;
    return os;
}

auto operator<<(std::ostream& os, const wait_result& value) -> std::ostream&
{
    if (value.holds_no_kids()) {
        os << value.no_kids();
    }
    else if (value.holds_error()) {
        os << value.error();
    }
    else if (value.holds_info()) {
        os << value.info();
    }
    return os;
}

namespace detail {

auto get_nohang_wait_option() noexcept -> wait_option
{
    return wait_option(WNOHANG);
}

}

auto wait(reference_process_id id, wait_option flags) noexcept
    -> wait_result
{
    auto status = 0;
    auto pid = decltype(::waitpid(pid_t(id), &status, int(flags))){};
    auto err = 0;
    do { // NOLINT(cppcoreguidelines-avoid-do-while)
        //itimerval new_timer{};
        //itimerval old_timer;
        //setitimer(ITIMER_REAL, &new_timer, &old_timer);
        pid = ::waitpid(pid_t(id), &status, int(flags));
        err = errno;
        //setitimer(ITIMER_REAL, &old_timer, nullptr);
    } while ((pid == -1) && (err == EINTR));
    if ((pid == -1) && (err == ECHILD)) {
        return wait_result::no_kids_t{};
    }
    if (pid == -1) {
        return wait_result::error_t(err);
    }
    if (WIFEXITED(status)) {
        // process terminated normally
        return wait_result::info_t{reference_process_id{pid},
            wait_exit_status{WEXITSTATUS(status)}};
    }
    if (WIFSIGNALED(status)) {
        // process terminated due to signal
        return wait_result::info_t{reference_process_id{pid},
            wait_signaled_status{WTERMSIG(status), WCOREDUMP(status) != 0}};
    }
    if (WIFSTOPPED(status)) {
        // process not terminated, but stopped and can be restarted
        return wait_result::info_t{reference_process_id{pid},
            wait_stopped_status{WSTOPSIG(status)}};
    }
    if (WIFCONTINUED(status)) {
        // process was resumed
        return wait_result::info_t{reference_process_id{pid},
            wait_continued_status{}};
    }
    return wait_result::info_t{reference_process_id{pid}};
}

auto wait(const system_name& name, instance& object)
    -> std::vector<wait_result>
{
    auto results = std::vector<wait_result>{};
    auto result = decltype(wait()){};
    while (bool(result = wait())) {
        if (handle(name, object, result)) {
            results.push_back(result);
        }
    }
    return results;
}

}
