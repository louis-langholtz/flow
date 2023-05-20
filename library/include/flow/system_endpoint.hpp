#ifndef system_endpoint_hpp
#define system_endpoint_hpp

#include <concepts> // for std::convertible_to
#include <ostream>
#include <set>
#include <utility> // for std::move
#include <type_traits> // for std::is_default_constructible_v

#include "flow/reference_descriptor.hpp"
#include "flow/system_name.hpp"

namespace flow {

struct system_endpoint
{
    explicit system_endpoint(system_name a = {},
                             std::set<reference_descriptor> d = {}):
        address{std::move(a)}, descriptors{std::move(d)} {}

    system_endpoint(system_name a,
                    std::convertible_to<reference_descriptor> auto&& ...d):
        address{std::move(a)}, descriptors{std::move(d)...} {}

    system_name address;

    ///@brief Well known descriptor ID of endpoint for systems.
    std::set<reference_descriptor> descriptors;
};

// Ensure regularity in terms of special member functions supported...
static_assert(std::is_default_constructible_v<system_endpoint>);
static_assert(std::is_copy_constructible_v<system_endpoint>);
static_assert(std::is_move_constructible_v<system_endpoint>);
static_assert(std::is_copy_assignable_v<system_endpoint>);
static_assert(std::is_move_assignable_v<system_endpoint>);

// Ensure initializing construction supported for contained types...
static_assert(std::is_constructible_v<system_endpoint,
              system_name>);
static_assert(std::is_constructible_v<system_endpoint,
              system_name, std::set<reference_descriptor>>);

// Ensure pack-expansion constructor works for descriptor_id's...
static_assert(std::is_constructible_v<system_endpoint,
              system_name, reference_descriptor>);
static_assert(std::is_constructible_v<system_endpoint,
              system_name, reference_descriptor, reference_descriptor>);

inline auto operator==(const system_endpoint& lhs,
                       const system_endpoint& rhs) -> bool
{
    return (lhs.address == rhs.address)
        && (lhs.descriptors == rhs.descriptors);
}

auto operator<<(std::ostream& os, const system_endpoint& value)
    -> std::ostream&;

}

#endif /* system_endpoint_hpp */
