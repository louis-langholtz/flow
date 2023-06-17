#ifndef node_endpoint_hpp
#define node_endpoint_hpp

#include <concepts> // for std::convertible_to
#include <istream>
#include <ostream>
#include <set>
#include <string_view>
#include <utility> // for std::move
#include <type_traits> // for std::is_constructible_v

#include "flow/port_id.hpp"
#include "flow/node_name.hpp"

namespace flow {

struct node_endpoint
{
    explicit node_endpoint(node_name a = {}): address{std::move(a)} {}

    node_endpoint(node_name a, std::set<port_id> d):
        address{std::move(a)}, ports{std::move(d)} {}

    node_endpoint(node_name a,
                    std::convertible_to<port_id> auto&& ...d):
        address{std::move(a)}, ports{std::move(d)...} {}

    /// @brief Well known name identifier for a node.
    node_name address;

    /// @brief Well known port IDs of endpoint for a node.
    std::set<port_id> ports;
};

inline auto operator==(const node_endpoint& lhs,
                       const node_endpoint& rhs) -> bool
{
    return (lhs.address == rhs.address)
        && (lhs.ports == rhs.ports);
}

static_assert(std::regular<node_endpoint>);

// Ensure initializing construction supported for contained types...
static_assert(std::is_constructible_v<node_endpoint,
              node_name>);
static_assert(std::is_constructible_v<node_endpoint,
              node_name, std::set<port_id>>);

// Ensure pack-expansion constructor works for descriptor_id's...
static_assert(std::is_constructible_v<node_endpoint,
              node_name, reference_descriptor>);
static_assert(std::is_constructible_v<node_endpoint,
              node_name, reference_descriptor, reference_descriptor>);

auto operator<<(std::ostream& os, const node_endpoint& value)
    -> std::ostream&;
auto operator>>(std::istream& is, node_endpoint& value) -> std::istream&;

auto to_ports(std::string_view string) -> std::set<port_id>;

}

#endif /* node_endpoint_hpp */
