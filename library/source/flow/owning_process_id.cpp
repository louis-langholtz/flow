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

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

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
        std::cerr << "got new wait status: " << impl.last_status << "\n";
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

using signal_handler =
    std::function<void(const boost::system::error_code& error, int sig)>;

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
    reference_process_id pid{current_process_id()};
    std::set<owning_process_id::impl*> impls;
    boost::asio::io_context io_context;
    boost::asio::signal_set signals{io_context};
    signal_handler handler;
    std::future<void> runner;
};

manager::manager()
{
    handler = [this](const boost::system::error_code& error, int sig) {
        if (error) {
            std::cerr << "got error " << error << "\n";
            return;
        }
        signals.async_wait(handler);
        std::cerr << flow::current_process_id();
        std::cerr << ": asio caught signal " << sig << '\n';
        if (sig != SIGCHLD) {
            return;
        }
        const auto opts = wait_options::nohang()|wait_options::untraced();
        for (;;) {
            const auto ret = wait(invalid_process_id, opts);
            if (std::holds_alternative<nokids_wait_result>(ret)) {
                break;
            }
            handle(ret);
        }
    };

    signals.add(SIGCHLD);
    signals.async_wait(handler);
    runner = std::async(std::launch::async, [&](){
        sigset_t new_set{};
        sigemptyset(&new_set);
        sigaddset(&new_set, SIGCHLD);
        pthread_sigmask(SIG_UNBLOCK, &new_set, nullptr);
        std::cerr << "runner start\n";
        io_context.run();
        std::cerr << "runner end\n";
    });
}

manager::~manager() noexcept
{
    if (pid == current_process_id()) {
        try {
            signals.cancel();
        }
        catch (const boost::system::system_error& ex) {
            std::cerr << "signals.cancel threw exception: ";
            std::cerr << ex.what();
            std::cerr << "\n";
        }
        catch (...) {
            std::cerr << "signals.cancel threw exception!\n";
        }
        try {
            io_context.stop();
        }
        catch (...) {
            std::cerr << "io_context.stop() threw exception\n";
        }
        if (runner.valid()) {
            try {
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
    const std::lock_guard<decltype(mutex)> lock{mutex};
    std::cerr << "insert " << size(impls);
    std::cerr << ", for " << pimpl->pid;
    std::cerr << ", by " << current_process_id();
    std::cerr << "\n";
    return impls.insert(pimpl).second;
}

auto manager::erase(owning_process_id::impl* pimpl) -> bool
{
    if (!pimpl) {
        return false;
    }
    const std::lock_guard<decltype(mutex)> lock{mutex};
    std::cerr << "erase " << size(impls) << ", by " << current_process_id() << "\n";
    return impls.erase(pimpl) > 0u;
}

auto manager::handle(const flow::info_wait_result& result) -> void
{
    std::cerr << "child signaled " << result << "\n";
    const std::lock_guard<decltype(mutex)> lock{mutex};
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
        const std::lock_guard<decltype(impl.mutex)> lock{impl.mutex};
        impl.statuses.push(result.status);
    }
    impl.cv.notify_one();
}

auto manager::handle(const flow::wait_result& result) -> void
{
    std::visit(detail::overloaded{
        [](auto) noexcept {},
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
        const std::lock_guard<decltype(impl.mutex)> lock{impl.mutex};
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
