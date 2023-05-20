#ifndef forwarding_channel_hpp
#define forwarding_channel_hpp

#include <cstdint> // for std::uintmax_t
#include <future>
#include <type_traits> // for std::is_default_constructible_v

#include "flow/descriptor.hpp"

namespace flow {

struct forwarding_channel
{
    struct counters
    {
        std::uintmax_t reads;
        std::uintmax_t writes;
        std::uintmax_t bytes;
    };

    forwarding_channel() = default;
    forwarding_channel(descriptor src_, descriptor dst_);

    [[nodiscard]] constexpr auto source() const noexcept
        -> reference_descriptor
    {
        return to_reference_descriptor(src);
    }

    [[nodiscard]] constexpr auto destination() const noexcept
        -> reference_descriptor
    {
        return to_reference_descriptor(dst);
    }

    [[nodiscard]] auto valid() const noexcept -> bool
    {
        return forwarder.valid();
    }

    auto get() -> counters
    {
        return forwarder.get();
    }

private:
    descriptor src;
    descriptor dst;
    std::future<counters> forwarder; // non-essential part!
};

constexpr auto operator==(const forwarding_channel& lhs,
                          const forwarding_channel& rhs) noexcept -> bool
{
    // Just checks essential parts...
    return (lhs.source() == rhs.source()) &&
           (lhs.destination() == rhs.destination());
}

auto operator<<(std::ostream& os, const forwarding_channel& value)
    -> std::ostream&;

static_assert(std::is_default_constructible_v<forwarding_channel>);
static_assert(std::is_move_constructible_v<forwarding_channel>);
static_assert(std::equality_comparable<forwarding_channel>);

}

#endif /* forwarding_channel_hpp */
