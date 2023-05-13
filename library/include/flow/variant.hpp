#ifndef variant_h
#define variant_h

#include <ostream>
#include <variant>

namespace flow {

/// @brief Variant type.
/// @note Use this alias instead of <code>std::variant</code> directly to
/// support output streaming within the same namespace as the flow object to
/// be streamed.
using std::variant;

template<class... Ts>
auto operator<<(std::ostream& os, const variant<Ts...>& sv) -> std::ostream&
{
    std::visit([&os](const auto& v) { os << v; }, sv);
    return os;
}

}

#endif /* variant_h */
