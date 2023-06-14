#ifndef system_hpp
#define system_hpp

#include <filesystem>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <type_traits> // for std::is_default_constructible_v
#include <vector>

#include "flow/connection.hpp"
#include "flow/port_map.hpp"
#include "flow/environment_map.hpp"
#include "flow/io_type.hpp"
#include "flow/system_name.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct system;

struct custom
{
    /// @brief Environment for the <code>system</code>.
    /// @note This is considered an _internal_ component of this type.
    environment_map environment;

    /// @brief Subsystems.
    /// @note The identifying key of a subsystem is considered an _external_
    ///   component of that subsystems.
    std::map<system_name, system> nodes;

    /// @brief Connections.
    /// @note The connections of this system are considered _internal_
    ///   components.
    std::vector<connection> links;
};

struct executable
{
    /// @brief Path to the executable file for this system.
    std::filesystem::path file;

    /// @brief Arguments to pass to the executable.
    std::vector<std::string> arguments;

    /// @brief Working directory.
    /// @todo Consider what sense this member makes & removing it if
    ///   there isn't enough reason to keep it around.
    std::filesystem::path working_directory;
};

/// @brief Recursive structure for defining systems.
/// @note Systems are conceptually made up of three categories of components:
///   _external_, _interface_, and _internal_.
/// @note Systems can be instantiated through calls to <code>instantiate</code>
///   into instances of the <code>instance</code> type.
/// @note This type is intended to be moveable, copyable, and equality
///   comparable.
/// @see instantiate, instance.
struct system
{
    system() = default;

    system(custom type_info,
           port_map des_map = {})
        : interface{std::move(des_map)},
          implementation{std::move(type_info)}
    {
        // Intentionally empty.
    }

    system(executable type_info,
           port_map des_map = std_ports)
        : interface{std::move(des_map)},
          implementation{std::move(type_info)}
    {
        // Intentionally empty.
    }

    /// @brief Ports of the <code>system</code>.
    /// @note This is considered an _interface_ component of this type.
    port_map interface;

    /// @brief System type specific information.
    /// @note This is considered an _internal_ component of this type.
    variant<custom, executable> implementation;
};

static_assert(std::is_default_constructible_v<system>);
static_assert(std::is_copy_constructible_v<system>);
static_assert(std::is_move_constructible_v<system>);
static_assert(std::is_copy_assignable_v<system>);
static_assert(std::is_move_assignable_v<system>);

inline auto operator==(const custom& lhs,
                       const custom& rhs) noexcept -> bool
{
    return (lhs.nodes == rhs.nodes)
        && (lhs.environment == rhs.environment)
        && (lhs.links == rhs.links);
}

inline auto operator==(const executable& lhs,
                       const executable& rhs) noexcept -> bool
{
    return (lhs.file == rhs.file)
        && (lhs.arguments == rhs.arguments)
        && (lhs.working_directory == rhs.working_directory);
}

inline auto operator==(const system& lhs,
                       const system& rhs) noexcept -> bool
{
    return (lhs.interface == rhs.interface)
        && (lhs.implementation == rhs.implementation);
}

auto operator<<(std::ostream& os, const system& value)
    -> std::ostream&;
auto pretty_print(std::ostream& os, const system& value) -> void;

auto get_matching_set(const system& sys, io_type io)
    -> std::set<port_id>;

/// @brief Makes connections for each of the specified ports with
///   <code>user_endpoint</code> on the other end.
/// @throws std::invalid_argument if a port map entry has a direction
///   other than <code>io_type::in</code>, <code>io_type::out</code> or
///   <code>io_type::bidir</code>.
auto connect_with_user(const system_name& name,
                       const port_map& ports)
    -> std::vector<connection>;

}

#endif /* system_hpp */
