#ifndef unset_endpoint_hpp
#define unset_endpoint_hpp

#include <ostream>

namespace flow {

/// @brief Unset endpoint.
/// @note Denotes an endpoint that is in a conceptually unset state.
struct unset_endpoint {
    constexpr auto operator<=>(const unset_endpoint&) const noexcept = default;
};

auto operator<<(std::ostream& os, const unset_endpoint&) -> std::ostream&;

}

#endif /* unset_endpoint_hpp */
