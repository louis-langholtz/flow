#ifndef descriptor_hpp
#define descriptor_hpp

#include <concepts> // for std::equality_comparable
#include <type_traits> // for std::is_default_constructible_v

#include "flow/owning_descriptor.hpp"
#include "flow/reference_descriptor.hpp"
#include "flow/variant.hpp"

namespace flow {

/// @brief Descripror.
/// @note A default constructed <code>reference_descriptor</code>'s underlying
///   value is 0. A default constructed <code>owning_descriptor</code>'s
///   underlying value meanwhile is -1. That's more sensible for a default
///   constructed instance of this type, so it's chosen as the first type of
///   the variant.
using descriptor = variant<
    owning_descriptor,
    reference_descriptor
>;

static_assert(std::is_default_constructible_v<descriptor>);
static_assert(std::equality_comparable<descriptor>);

constexpr auto to_reference_descriptor(const descriptor& d) noexcept
    -> reference_descriptor
{
    if (const auto p = std::get_if<owning_descriptor>(&d)) {
        return reference_descriptor(*p);
    }
    if (const auto p = std::get_if<reference_descriptor>(&d)) {
        return *p;
    }
    return descriptors::invalid_id;
}

}

#endif /* descriptor_hpp */
