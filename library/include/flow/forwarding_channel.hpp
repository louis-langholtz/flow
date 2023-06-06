#ifndef forwarding_channel_hpp
#define forwarding_channel_hpp

#include <cstdint> // for std::uintmax_t
#include <experimental/propagate_const>
#include <memory> // for std::unique_ptr
#include <type_traits> // for std::is_default_constructible_v

#include "flow/descriptor.hpp"

namespace flow {

struct forwarding_channel
{
    struct impl;

    struct counters
    {
        std::uintmax_t reads;
        std::uintmax_t writes;
        std::uintmax_t bytes;
    };

    forwarding_channel();
    forwarding_channel(descriptor src_, descriptor dst_);
    forwarding_channel(const forwarding_channel& other) = delete;
    forwarding_channel(forwarding_channel&& other) noexcept;
    ~forwarding_channel();
    auto operator=(const forwarding_channel& other)
        -> forwarding_channel& = delete;
    auto operator=(forwarding_channel&& other) noexcept
        -> forwarding_channel&;

    [[nodiscard]] auto source() const noexcept -> reference_descriptor;

    [[nodiscard]] auto destination() const noexcept -> reference_descriptor;

    [[nodiscard]] auto valid() const noexcept -> bool;

    auto get_result() -> counters;

private:
    std::experimental::propagate_const<std::unique_ptr<impl>> pimpl;
};

inline auto operator==(const forwarding_channel& lhs,
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
