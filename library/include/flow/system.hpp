#ifndef system_hpp
#define system_hpp

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/environment_map.hpp"
#include "flow/io_type.hpp"
#include "flow/system_name.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct descriptor_info {
    std::string comment;
    io_type direction;
};

using descriptor_map = std::map<descriptor_id, descriptor_info>;

auto operator<<(std::ostream& os,
                const descriptor_map& value)
    -> std::ostream&;

const auto std_descriptors = descriptor_map{
    {descriptor_id{0}, {"stdin", io_type::in}},
    {descriptor_id{1}, {"stdout", io_type::out}},
    {descriptor_id{2}, {"stderr", io_type::out}},
};

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

    system(custom type_info, descriptor_map map = {})
        : descriptors{std::move(map)},
          info{std::move(type_info)}
    {
        // Intentionally empty.
    }

    system(executable type_info, descriptor_map map = std_descriptors):
        descriptors{std::move(map)},
        info{std::move(type_info)}
    {
        // Intentionally empty.
    }

    descriptor_map descriptors;
    environment_map environment;
    variant<custom, executable> info;
};

auto operator<<(std::ostream& os, const system& value)
    -> std::ostream&;

}

#endif /* system_hpp */
