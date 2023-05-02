#ifndef prototype_hpp
#define prototype_hpp

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "flow/descriptor_id.hpp"
#include "flow/connection.hpp"
#include "flow/io_type.hpp"
#include "flow/system_name.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct descriptor_info {
    std::string comment;
    io_type direction;
};

using descriptor_container = std::map<descriptor_id, descriptor_info>;

inline const auto standard_descriptors = descriptor_container{
    {descriptor_id{0}, {"stdin", io_type::in}},
    {descriptor_id{1}, {"stdout", io_type::out}},
    {descriptor_id{2}, {"stderr", io_type::out}},
};

struct custom_system;
struct executable_system;

using system = variant<
    custom_system,
    executable_system
>;

using argument_container = std::vector<std::string>;
using environment_container = std::map<std::string, std::string>;

struct executable_system {
    descriptor_container descriptors{standard_descriptors};
    std::filesystem::path executable_file;
    argument_container arguments;
    std::filesystem::path working_directory;
    environment_container environment;
};

struct custom_system {
    descriptor_container descriptors{standard_descriptors};
    std::map<system_name, system> prototypes;
    std::vector<connection> connections;
};

auto operator<<(std::ostream& os, const descriptor_container& value)
    -> std::ostream&;
auto operator<<(std::ostream& os, const executable_system& value)
    -> std::ostream&;
auto operator<<(std::ostream& os, const custom_system& value)
    -> std::ostream&;

}

#endif /* prototype_hpp */
