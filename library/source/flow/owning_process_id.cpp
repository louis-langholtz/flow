#include <unistd.h> // for pid_t
#include <sys/wait.h>

#include <cerrno> // for errno
#include <csignal>
#include <utility> // for std::exchange

#include "flow/owning_process_id.hpp"

namespace flow {

auto owning_process_id::fork() -> owning_process_id
{
    return owning_process_id{reference_process_id(::fork())};
}

owning_process_id::owning_process_id(owning_process_id&& other) noexcept
    : pid(std::exchange(other.pid, default_process_id))
{
    // Intentionally empty.
}

owning_process_id::~owning_process_id()
{
    while (int(pid) > 0) {
        wait(wait_option{});
    }
}

auto owning_process_id::operator=(owning_process_id&& other) noexcept
    -> owning_process_id&
{
    if (this != &other) {
        pid = std::exchange(other.pid, default_process_id);
    }
    return *this;
}

auto owning_process_id::wait(wait_option flags) noexcept -> wait_result
{
    const auto result = flow::wait(pid, flags);
    switch (result.type()) {
    case wait_result::has_error:
        return result;
    case wait_result::no_children:
        pid = default_process_id;
        break;
    case wait_result::has_info: {
        const auto info = result.info();
        if (info.id == pid) {
            if (std::holds_alternative<wait_exit_status>(info.status)) {
                pid = default_process_id;
            }
            else if (std::holds_alternative<wait_signaled_status>(info.status)) {
                pid = default_process_id;
            }
        }
        break;
    }
    }
    return result;
}

}
