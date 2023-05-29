#include <sys/wait.h>

#include <csignal> // for kill

#include "flow/instance.hpp"
#include "flow/system_name.hpp"
#include "flow/utility.hpp"
#include "flow/wait_result.hpp"

namespace flow {

namespace {

auto wait(instance::forked& instance) -> std::vector<wait_result>
{
    return std::visit(detail::overloaded{
        [&instance](const owning_process_id& id){
            auto results = std::vector<wait_result>{};
            for (;;) {
                auto result = wait(reference_process_id(id));
                if (std::holds_alternative<nokids_wait_result>(result)) {
                    break;
                }
                if (std::holds_alternative<info_wait_result>(result)) {
                    instance.state = std::get<info_wait_result>(result).status;
                    results.push_back(result);
                    break;
                }
                if (std::holds_alternative<error_wait_result>(result)) {
                    results.push_back(result);
                    continue;
                }
            }
            return results;
        },
        [](const wait_status&){
            return std::vector<wait_result>{};
        }
    }, instance.state);
}

}

auto operator<<(std::ostream& os, const empty_wait_result&)
    -> std::ostream&
{
    os << "empty wait result";
    return os;
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

auto wait(reference_process_id id, wait_option flags) noexcept
    -> wait_result
{
    auto status = 0;
    auto pid = decltype(::waitpid(pid_t(id), &status, int(flags))){};
    auto err = 0;
    if (pid_t(id) > 0) {
        sigsafe_counter_reset();
    }
    auto sig = SIGINT;
    for (;;) {
        pid = ::waitpid(pid_t(id), &status, int(flags));
        err = errno;
        if (pid != -1) {
            break;
        }
        if (err != EINTR) {
            break;
        }
        std::cerr << "waitpid(" << id << ") interrupted by signal\n";
        if (pid_t(id) > 0) {
            if (sigsafe_counter_take()) {
                ::kill(pid_t(id), sig);
                sig = SIGKILL;
            }
        }
    }
    if (pid < 0) { // treat all negeatives as error
        if (err == ECHILD) {
            return nokids_wait_result{};
        }
        return error_wait_result{os_error_code(err)};
    }
    if (pid == 0) {
        return empty_wait_result{};
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
        // See "man 4 termios" for more info. From "man 2 waitpid":
        // Process not terminated, but stopped due to a SIGTTIN, SIGTTOU,
        // SIGTSTP, or SIGSTOP signal and can be restarted.
        // Can be true only if wait call specified WUNTRACED option or if
        // child process is being traced.
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
    return std::visit(detail::overloaded{
        [](instance::forked& obj){
            return wait(obj);
        },
        [](instance::custom& obj){
            auto results = std::vector<wait_result>{};
            for (auto&& entry: obj.children) {
                const auto waits = wait(entry.second);
                results.insert(end(results), begin(waits), end(waits));
            }
            return results;
        },
    }, object.info);
}

}
