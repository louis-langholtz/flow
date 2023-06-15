#ifndef system_hpp
#define system_hpp

#include <map>
#include <ostream>
#include <type_traits> // for std::is_default_constructible_v
#include <vector>

#include "flow/environment_map.hpp"
#include "flow/link.hpp"
#include "flow/node_name.hpp"

namespace flow {

struct node;

/// @brief Recursive structure for defining systems.
/// @note This is a <code>node</code> implementation type.
/// @see node.
struct system
{
    /// @brief Environment for the <code>system</code>.
    environment_map environment;

    /// @brief Nodes.
    /// @note The identifying key of a node is considered an _external_
    ///   component of that node.
    std::map<node_name, node> nodes;

    /// @brief Links.
    std::vector<link> links;
};

static_assert(std::is_default_constructible_v<system>);
static_assert(std::is_copy_constructible_v<system>);
static_assert(std::is_move_constructible_v<system>);
static_assert(std::is_copy_assignable_v<system>);
static_assert(std::is_move_assignable_v<system>);

auto operator==(const system& lhs, const system& rhs) noexcept -> bool;

auto operator<<(std::ostream& os, const system& value) -> std::ostream&;

}

#endif /* system_hpp */
