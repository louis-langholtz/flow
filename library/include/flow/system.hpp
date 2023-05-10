#ifndef system_hpp
#define system_hpp

#include <map>
#include <ostream>
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

struct system
{
    struct custom
    {
        std::map<system_name, system> subsystems;
        std::vector<connection> connections;
    };

    struct executable
    {
        std::filesystem::path executable_file;
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

auto operator<<(std::ostream& os, const system& value)
    -> std::ostream&;

}

#endif /* system_hpp */
