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
#include "flow/descriptor_map.hpp"
#include "flow/environment_map.hpp"
#include "flow/io_type.hpp"
#include "flow/system_name.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

/// @brief Recursive structure for defining systems.
/// @note Systems can be instantiated through calls to <code>instantiate</code>
///   into instances of the <code>instance</code> type.
/// @note This type is intended to be moveable, copyable, and equality
///   comparable.
/// @see instantiate, instance.
struct system
{
    struct custom
    {
        std::map<system_name, system> subsystems;
        std::vector<connection> connections;
    };

    struct executable
    {
        std::filesystem::path file;
        std::vector<std::string> arguments;
        std::filesystem::path working_directory;
    };

    system() = default;

    system(custom type_info,
           descriptor_map des_map = {},
           environment_map env_map = {})
        : descriptors{std::move(des_map)},
          environment{std::move(env_map)},
          info{std::move(type_info)}
    {
        // Intentionally empty.
    }

    system(executable type_info,
           descriptor_map des_map = std_descriptors,
           environment_map env_map = {})
        : descriptors{std::move(des_map)},
          environment{std::move(env_map)},
          info{std::move(type_info)}
    {
        // Intentionally empty.
    }

    descriptor_map descriptors;
    environment_map environment;
    variant<custom, executable> info;
};

static_assert(std::is_default_constructible_v<system>);
static_assert(std::is_copy_constructible_v<system>);
static_assert(std::is_move_constructible_v<system>);
static_assert(std::is_copy_assignable_v<system>);
static_assert(std::is_move_assignable_v<system>);

inline auto operator==(const system::custom& lhs,
                       const system::custom& rhs) noexcept -> bool
{
    return (lhs.subsystems == rhs.subsystems)
        && (lhs.connections == rhs.connections);
}

inline auto operator==(const system::executable& lhs,
                       const system::executable& rhs) noexcept -> bool
{
    return (lhs.file == rhs.file)
        && (lhs.arguments == rhs.arguments)
        && (lhs.working_directory == rhs.working_directory);
}

inline auto operator==(const system& lhs,
                       const system& rhs) noexcept -> bool
{
    return (lhs.descriptors == rhs.descriptors)
        && (lhs.environment == rhs.environment)
        && (lhs.info == rhs.info);
}

auto operator<<(std::ostream& os, const system& value)
    -> std::ostream&;
auto pretty_print(std::ostream& os, const system& value) -> void;

auto get_matching_set(const system& sys, io_type io)
    -> std::set<reference_descriptor>;

/// @brief Makes connections for each of the specified descriptors with
///   <code>user_endpoint</code> on the other end.
/// @throws std::invalid_argument if a descriptor map entry has a direction
///   other than <code>io_type::in</code>, <code>io_type::out</code> or
///   <code>io_type::bidir</code>.
auto connect_with_user(const system_name& name,
                       const descriptor_map& descriptors)
    -> std::vector<connection>;

}

#endif /* system_hpp */
