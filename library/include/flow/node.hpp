#ifndef node_hpp
#define node_hpp

#include <concepts> // for std::regular.
#include <ostream>
#include <set>

#include "flow/executable.hpp"
#include "flow/io_type.hpp"
#include "flow/link.hpp"
#include "flow/node_name.hpp"
#include "flow/port_map.hpp"
#include "flow/system.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

/// @brief Recursive structure for defining nodes.
/// @note Nodes are conceptually made up of three categories of components:
///   _external_, _interface_, and _internal_.
/// @note Nodes can be instantiated through calls to <code>instantiate</code>
///   into instances of the <code>instance</code> type.
/// @note This type is intended to be moveable, copyable, and equality
///   comparable.
/// @see instantiate, instance.
struct node
{
    node() = default;

    node(system type_info, port_map des_map = {})
        : interface{std::move(des_map)},
          implementation{std::move(type_info)}
    {
        // Intentionally empty.
    }

    node(executable type_info, port_map des_map = std_ports)
        : interface{std::move(des_map)},
          implementation{std::move(type_info)}
    {
        // Intentionally empty.
    }

    /// @brief Ports of the <code>node</code>.
    /// @note This is considered an _interface_ component of this type.
    port_map interface;

    /// @brief Implementation specific information.
    /// @note This is considered an _internal_ component of this type.
    variant<system, executable> implementation;
};

inline auto operator==(const node& lhs,
                       const node& rhs) noexcept -> bool
{
    return (lhs.interface == rhs.interface)
        && (lhs.implementation == rhs.implementation);
}

// Ensure regularity...
static_assert(std::regular<node>);

auto operator<<(std::ostream& os, const node& value)
    -> std::ostream&;
auto pretty_print(std::ostream& os, const node& value) -> void;

auto get_matching_set(const node& sys, io_type io)
    -> std::set<port_id>;

/// @brief Makes links for each of the specified ports with
///   <code>user_endpoint</code> on the other end.
/// @throws std::invalid_argument if a port map entry has a direction
///   other than <code>io_type::in</code>, <code>io_type::out</code> or
///   <code>io_type::bidir</code>.
auto link_with_user(const node_name& name, const port_map& ports)
    -> std::vector<link>;

}

#endif /* node_hpp */
