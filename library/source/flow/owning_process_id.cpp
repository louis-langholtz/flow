#include <pthread.h>
#include <unistd.h> // for pid_t

#include <sys/wait.h>

#include <cerrno> // for errno
#include <condition_variable>
#include <csignal>
#include <functional> // for std::reference_wrapper
#include <future>
#include <mutex>
#include <queue>
#include <set>
#include <utility> // for std::exchange

#include "flow/owning_process_id.hpp"
#include "flow/utility.hpp"

namespace flow {

struct owning_process_id::impl
{
    impl(reference_process_id id);
    ~impl() noexcept;

    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    reference_process_id pid{default_process_id};
    std::queue<wait_status> statuses;
    wait_status last_status{default_status};
};

static_assert(!std::is_default_constructible_v<owning_process_id::impl>);

auto wait(owning_process_id::impl& impl, wait_option flags = wait_option{})
    -> wait_status
{
    std::unique_lock lk(impl.mutex);
    const auto pid = impl.pid;
    switch (pid) {
    case invalid_process_id:
    case no_process_id:
        return impl.last_status;
    }
    if ((flags & wait_options::nohang()) != wait_option{}) {
        if (!empty(impl.statuses)) {
            impl.last_status = impl.statuses.front();
            impl.statuses.pop();
        }
    }
    if (std::holds_alternative<wait_exit_status>(impl.last_status) ||
        std::holds_alternative<wait_signaled_status>(impl.last_status)) {
        impl.pid = owning_process_id::default_process_id;
        return impl.last_status;
    };
    for (;;) {
        impl.cv.wait(lk, [&impl]{
            return !empty(impl.statuses);
        });
        impl.last_status = impl.statuses.front();
        impl.statuses.pop();
        if (std::holds_alternative<wait_exit_status>(impl.last_status) ||
            std::holds_alternative<wait_signaled_status>(impl.last_status)) {
            impl.pid = owning_process_id::default_process_id;
            return impl.last_status;
        };
        if ((flags & wait_options::untraced()) != wait_option{}) {
            return impl.last_status;
        }
        if (!std::holds_alternative<wait_stopped_status>(impl.last_status) &&
            !std::holds_alternative<wait_continued_status>(impl.last_status)) {
            return impl.last_status;
        }
    }
}

namespace {

auto count_alive(const std::set<owning_process_id::impl*>& impls)
    -> std::size_t
{
    auto count = std::size_t{};
    for (auto&& impl: impls) {
        const std::lock_guard<decltype(impl->mutex)> lk{impl->mutex};
        if (impl->pid > no_process_id) {
            ++count;
        }
    }
    return count;
}

struct manager
{
    manager();
    ~manager() noexcept;
    auto handle(const flow::info_wait_result& result) -> void;
    auto handle(const flow::wait_result& result) -> void;
    auto insert(owning_process_id::impl* pimpl) -> bool;
    auto erase(owning_process_id::impl* pimpl) -> bool;

private:
    std::mutex mutex;
    std::condition_variable cv;
    reference_process_id pid{current_process_id()};
    std::set<owning_process_id::impl*> impls;
    std::atomic_bool do_run{true};
    std::future<void> runner;
};

manager::manager()
{
    set_signal_handler(signals::child());
    runner = std::async(std::launch::async, [&](){
        const auto opts = wait_options::untraced();
        while (do_run) {
            handle(wait(invalid_process_id, opts));
        }
    });
}

manager::~manager() noexcept
{
    if (pid == current_process_id()) {
        if (runner.valid()) {
            try {
                do_run = false;
                cv.notify_all();
                runner.get();
            }
            catch (...) {
                std::cerr << "runner.get() threw exception\n";
            }
        }
    }
}

auto manager::insert(owning_process_id::impl* pimpl) -> bool
{
    if (!pimpl || (reference_process_id(pimpl->pid) <= no_process_id)) {
        return false;
    }
    auto first = false;
    {
        const std::lock_guard lock{mutex};
        first = empty(impls);
        if (!impls.insert(pimpl).second) {
            return false;
        }
    }
    if (first) {
        cv.notify_one();
    }
    return true;
}

auto manager::erase(owning_process_id::impl* pimpl) -> bool
{
    if (!pimpl) {
        return false;
    }
    const std::lock_guard lock{mutex};
    return impls.erase(pimpl) > 0u;
}

auto manager::handle(const flow::info_wait_result& result) -> void
{
    const std::lock_guard lock{mutex};
    const auto it = find_if(begin(impls), end(impls),
                            [&result](const owning_process_id::impl *pimpl){
        return pimpl->pid == result.id;
    });
    if (it == end(impls)) {
        std::cerr << "not found in impls list?!\n";
        return;
    }
    auto& impl = *(*it);
    {
        const std::lock_guard lock{impl.mutex};
        impl.statuses.push(result.status);
    }
    impl.cv.notify_one();
}

auto manager::handle(const flow::wait_result& result) -> void
{
    std::visit(detail::overloaded{
        [](auto) noexcept {},
        [&](const nokids_wait_result&) {
            std::unique_lock lk(mutex);
            cv.wait(lk, [this]{
                return (count_alive(impls) > 0) || !do_run;
            });
        },
        [](const error_wait_result& result) {
            std::cerr << result << ": odd?\n";
        },
        [this](const info_wait_result& results) {
            handle(results);
        },
    }, result);
}

auto the_manager() -> manager&
{
    static manager singleton;
    return singleton;
}

}

auto owning_process_id::fork() -> reference_process_id
{
    the_manager();
    return reference_process_id{::fork()};
}

owning_process_id::impl::impl(reference_process_id id): pid{id}
{
    the_manager().insert(this);
}

owning_process_id::impl::~impl() noexcept
{
    for (;;) {
        if (pid == invalid_process_id || pid == no_process_id) {
            break;
        }
        try {
            (void) ::flow::wait(*this);
        }
        catch (...) {
            std::cerr << "impl::~impl()";
            std::cerr << ": call to ::flow::wait(*this) threw exception\n";
        }
    }
    try {
        the_manager().erase(this);
    }
    catch (...) {
        std::cerr << "impl::~impl()";
        std::cerr << ": call to the_manager().erase(this) threw exception\n";
    }
}

owning_process_id::owning_process_id()
{
    the_manager();
}

owning_process_id::owning_process_id(reference_process_id id):
    pimpl{(id <= no_process_id)? nullptr: std::make_unique<impl>(id)}
{
    // Intentionally empty.
}

owning_process_id::owning_process_id(owning_process_id&& other) noexcept =
    default;

// Need destructor defaulted here, to avoid owning_process_id::impl being
// incomplete type for unique_ptr usage!
owning_process_id::~owning_process_id() = default; // NOLINT(performance-trivially-destructible)

owning_process_id::operator reference_process_id() const noexcept
{
    return pimpl? pimpl->pid: default_process_id;
}

auto owning_process_id::operator<=>(const owning_process_id& other) const noexcept
{
    return reference_process_id(*this) <=> reference_process_id(other);
}

auto owning_process_id::status() const noexcept -> wait_status
{
    if (pimpl) {
        auto& impl = *pimpl;
        const std::lock_guard lock{impl.mutex};
        return empty(impl.statuses)? default_status: impl.statuses.front();
    }
    return default_status;
}

auto owning_process_id::operator=(owning_process_id&& other) noexcept
    -> owning_process_id& = default;

auto owning_process_id::wait(wait_option flags) noexcept -> wait_status
{
    return pimpl? ::flow::wait(*pimpl, flags): default_status;
}

}
