#include <array>
#include <future>
#include <mutex>
#include <sstream> // for std::ostringstream

#include <unistd.h> // for read, write

#include "flow/forwarding_channel.hpp"

namespace flow {

struct forwarding_channel::impl
{
    impl(descriptor src_, descriptor dst_):
        src{std::move(src_)},
        dst{std::move(dst_)},
        forwarder{std::async(std::launch::async, &impl::relay, this)}
    {
        // Intentionally empty.
    }

    auto relay() -> counters
    {
        static constexpr auto buffer_size = 4096u;
        const auto from = int(to_reference_descriptor(src));
        const auto to = int(to_reference_descriptor(dst));
        auto stats = counters{};
        std::array<char, buffer_size> buffer{};
        for (;;) {
            const auto nread = ::read(from, data(buffer), size(buffer));
            if (nread == -1) {
                const auto err = os_error_code(errno);
                std::ostringstream os;
                os << "read from descriptor " << from << " failed: ";
                throw_error(err, os.str());
            }
            ++stats.reads;
            if (nread == 0) {
                break;
            }
            auto offset = decltype(::write(to, data(buffer), 0)){};
            auto remaining = nread;
            while (remaining > 0) {
                const auto nwrite = ::write(to, data(buffer) + offset, remaining);
                if (nwrite == -1) {
                    const auto err = os_error_code(errno);
                    std::ostringstream os;
                    os << "write to descriptor " << to << " failed: ";
                    throw_error(err, os.str());
                }
                ++stats.writes;
                offset += nwrite;
                remaining -= nwrite;
            }
            stats.bytes += static_cast<std::uintmax_t>(nread);
            // Time for taking this lock should be dwarfed by time for read
            // and writes.
            const std::lock_guard lock{mutex};
            counts = stats;
        }
        return stats;
    }

    auto get_progress() const -> counters
    {
        const std::lock_guard lock{mutex};
        return counts;
    }

    descriptor src;
    descriptor dst;

    // non-essential parts...
    mutable std::mutex mutex;
    counters counts{};
    std::future<counters> forwarder;
};

forwarding_channel::forwarding_channel() = default;

forwarding_channel::forwarding_channel(descriptor src_, descriptor dst_)
    : pimpl{std::make_unique<impl>(std::move(src_), std::move(dst_))}
{
}

forwarding_channel::forwarding_channel(forwarding_channel&& other) noexcept =
    default;

forwarding_channel::~forwarding_channel() = default;

auto forwarding_channel::operator=(forwarding_channel&& other) noexcept
    -> forwarding_channel& = default;

auto forwarding_channel::source() const noexcept
    -> reference_descriptor
{
    return pimpl? to_reference_descriptor(pimpl->src): descriptors::invalid_id;
}

auto forwarding_channel::destination() const noexcept
    -> reference_descriptor
{
    return pimpl? to_reference_descriptor(pimpl->dst): descriptors::invalid_id;
}

auto forwarding_channel::valid() const noexcept -> bool
{
    return pimpl? pimpl->forwarder.valid(): false;
}

auto forwarding_channel::get_progress() const -> counters
{
    return pimpl? pimpl->get_progress(): counters{};
}

auto forwarding_channel::get_result() -> counters
{
    return pimpl? pimpl->forwarder.get(): counters{};
}

auto operator<<(std::ostream& os, const forwarding_channel& value)
    -> std::ostream&
{
    os << "forwarding_channel{";
    os << value.source();
    os << ",";
    os << value.destination();
    os << "}";
    return os;
}

}
