#include <unistd.h> // for pid_t
#include <signal.h>
#include <sys/wait.h>

#include <cerrno> // for errno
#include <utility> // for std::exchange

#include "flow/owning_process_id.hpp"

namespace flow {

owning_process_id owning_process_id::fork()
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
    if (int(pid) > 0) {
        int status{};
        const auto id = ::waitpid(pid_t(pid), &status, WNOHANG);
        switch (id) {
        case -1: {
            auto err = errno;
            switch (err) {
            case ECHILD:
                return; // all is well
            default:
                break;
            }
            // not much can be done.
            break;
        }
        case 0: {
            ::kill(pid_t(pid), SIGTERM);
            ::waitpid(pid_t(pid), &status, 0);
            break;
        }
        }
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

}
