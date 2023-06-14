#ifndef link_hpp
#define link_hpp

#include <array>
#include <concepts> // for std::convertible_to
#include <ostream>
#include <type_traits> // for std::is_default_constructible_v

#include "flow/endpoint.hpp"
#include "flow/variant.hpp"

namespace flow {

struct unidirectional_link {
    endpoint src;
    endpoint dst;
};

constexpr auto operator==(const unidirectional_link& lhs,
                          const unidirectional_link& rhs) noexcept
    -> bool
{
    return (lhs.src == rhs.src) && (lhs.dst == rhs.dst);
}

auto operator<<(std::ostream& os, const unidirectional_link& value)
    -> std::ostream&;

struct bidirectional_link {
    std::array<endpoint, 2u> ends;
};

constexpr auto operator==(const bidirectional_link& lhs,
                          const bidirectional_link& rhs) noexcept
    -> bool
{
    return lhs.ends == rhs.ends;
}

auto operator<<(std::ostream& os, const bidirectional_link& value)
    -> std::ostream&;

using link = variant<
    unidirectional_link,
    bidirectional_link
>;

static_assert(std::is_default_constructible_v<link>);
static_assert(std::equality_comparable<link>);

template <std::convertible_to<endpoint> T>
auto make_endpoints(const unidirectional_link& c)
    -> std::array<const T*, 2u>
{
    return {std::get_if<T>(&c.src), std::get_if<T>(&c.dst)};
}

template <std::convertible_to<endpoint> T>
auto make_endpoints(const bidirectional_link& c)
    -> std::array<const T*, 2u>
{
    return {
        std::get_if<T>(&c.ends[0]), // NOLINT(readability-container-data-pointer)
        std::get_if<T>(&c.ends[1])
    };
}

template <std::convertible_to<endpoint> T>
auto make_endpoints(const link& c) -> std::array<const T*, 2u>
{
    if (const auto p = std::get_if<unidirectional_link>(&c)) {
        return make_endpoints<T>(*p);
    }
    if (const auto p = std::get_if<bidirectional_link>(&c)) {
        return make_endpoints<T>(*p);
    }
    return std::array<const T*, 2u>{nullptr, nullptr};
}

}

#endif /* link_hpp */
