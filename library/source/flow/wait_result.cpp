#include <sys/wait.h>

#include "flow/instance.hpp"
#include "flow/system_name.hpp"
#include "flow/utility.hpp"
#include "flow/wait_result.hpp"

namespace flow {

namespace {

auto find(instance& object, const reference_process_id& pid)
    -> std::optional<decltype(std::ref(object))>
{
    if (const auto p = std::get_if<instance::forked>(&object.info)) {
        if (const auto q = std::get_if<owning_process_id>(&p->state)) {
            if (reference_process_id(*q) == pid) {
                return std::ref(object);
            }
        }
        return {};
    }
    if (const auto p = std::get_if<instance::custom>(&object.info)) {
        for (auto&& entry: p->children) {
            if (auto found = find(entry.second, pid)) {
                return found;
            }
        }
    }
    return {};
}

auto handle(instance& instance, const info_wait_result& info) -> bool
{
    static const auto unknown_name = system_name{"unknown"};
    const auto entry = find(instance, info.id);
    std::visit(detail::overloaded{
        [](const wait_unknown_status&) {},
        [&entry](const wait_exit_status& arg) {
            if (entry) {
                (std::get<instance::forked>(entry->get().info)).state =
                    wait_status{arg};
            }
        },
        [&entry](const wait_signaled_status& arg) {
            if (entry) {
                (std::get<instance::forked>(entry->get().info)).state =
                    wait_status{arg};
            }
        },
        [](const wait_stopped_status&){},
        [](const wait_continued_status&){},
    }, info.status);
    return true;
}

auto handle(instance& instance, const wait_result& result) -> bool
{
    return std::visit(detail::overloaded{
        [](const nokids_wait_result&){
            return false;
        },
        [](const error_wait_result&){
            return true;
        },
        [&instance](const info_wait_result& arg){
            return handle(instance, arg);
        },
    }, result);
}

}

auto operator<<(std::ostream& os, const nokids_wait_result&)
    -> std::ostream&
{
    os << "no child processes to wait for";
    return os;
}

auto operator<<(std::ostream& os, const error_wait_result& arg)
    -> std::ostream&
{
    os << arg.data;
    return os;
}

auto operator<<(std::ostream& os, const info_wait_result& arg)
    -> std::ostream&
{
    os << arg.id << ", " << arg.status;
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
        return nokids_wait_result{};
    }
    if (pid == -1) {
        return error_wait_result{os_error_code(err)};
    }
    if (WIFEXITED(status)) {
        // process terminated normally
        return info_wait_result{reference_process_id{pid},
            wait_exit_status{WEXITSTATUS(status)}};
    }
    if (WIFSIGNALED(status)) {
        // process terminated due to signal
        return info_wait_result{reference_process_id{pid},
            wait_signaled_status{WTERMSIG(status), WCOREDUMP(status) != 0}};
    }
    if (WIFSTOPPED(status)) {
        // process not terminated, but stopped and can be restarted
        return info_wait_result{reference_process_id{pid},
            wait_stopped_status{WSTOPSIG(status)}};
    }
    if (WIFCONTINUED(status)) {
        // process was resumed
        return info_wait_result{reference_process_id{pid},
            wait_continued_status{}};
    }
    return info_wait_result{reference_process_id{pid}};
}

auto wait(instance& object) -> std::vector<wait_result>
{
    auto results = std::vector<wait_result>{};
    auto result = decltype(wait()){};
    while (!std::holds_alternative<nokids_wait_result>(result = wait())) {
        if (handle(object, result)) {
            results.push_back(result);
        }
    }
    return results;
}

}
