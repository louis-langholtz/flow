#ifndef link_hpp
#define link_hpp

#include <array>
#include <concepts> // for std::convertible_to, std::regular
#include <ostream>
#include <utility> // for std::move

#include "flow/endpoint.hpp"

namespace flow {

struct link
{
    link() = default;

    template <std::convertible_to<endpoint> A, std::convertible_to<endpoint> B>
    link(A a_, B b_): a(std::move(a_)), b(std::move(b_)) {}

    endpoint a;
    endpoint b;
};

constexpr auto operator==(const link& lhs, const link& rhs) noexcept -> bool
{
    return (lhs.a == rhs.a) && (lhs.b == rhs.b);
}

static_assert(std::regular<link>);

auto operator<<(std::ostream& os, const link& value) -> std::ostream&;

template <std::convertible_to<endpoint> T>
auto make_endpoints(const link& c) -> std::array<const T*, 2u>
{
    return {std::get_if<T>(&c.a), std::get_if<T>(&c.b)};
}

}

#endif /* link_hpp */
