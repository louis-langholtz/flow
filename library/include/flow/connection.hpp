#ifndef connection_hpp
#define connection_hpp

#include <array>
#include <concepts> // for std::convertible_to
#include <ostream>

#include "flow/endpoint.hpp"
#include "flow/variant.hpp"

namespace flow {

struct unidirectional_connection {
    endpoint src;
    endpoint dst;
};

constexpr auto operator==(const unidirectional_connection& lhs,
                          const unidirectional_connection& rhs) noexcept
    -> bool
{
    return (lhs.src == rhs.src) && (lhs.dst == rhs.dst);
}

auto operator<<(std::ostream& os, const unidirectional_connection& value)
    -> std::ostream&;

struct bidirectional_connection {
    std::array<endpoint, 2u> ends;
};

constexpr auto operator==(const bidirectional_connection& lhs,
                          const bidirectional_connection& rhs) noexcept
    -> bool
{
    return lhs.ends == rhs.ends;
}

auto operator<<(std::ostream& os, const bidirectional_connection& value)
    -> std::ostream&;

using connection = variant<
    unidirectional_connection,
    bidirectional_connection
>;

template <std::convertible_to<endpoint> T>
auto make_endpoints(const unidirectional_connection& c)
    -> std::array<const T*, 2u>
{
    return {std::get_if<T>(&c.src), std::get_if<T>(&c.dst)};
}

template <std::convertible_to<endpoint> T>
auto make_endpoints(const bidirectional_connection& c)
    -> std::array<const T*, 2u>
{
    return {
        std::get_if<T>(&c.ends[0]), // NOLINT(readability-container-data-pointer)
        std::get_if<T>(&c.ends[1])
    };
}

template <std::convertible_to<endpoint> T>
auto make_endpoints(const connection& c) -> std::array<const T*, 2u>
{
    if (const auto p = std::get_if<unidirectional_connection>(&c)) {
        return make_endpoints<T>(*p);
    }
    if (const auto p = std::get_if<bidirectional_connection>(&c)) {
        return make_endpoints<T>(*p);
    }
    return std::array<const T*, 2u>{nullptr, nullptr};
}

}

#endif /* connection_hpp */
