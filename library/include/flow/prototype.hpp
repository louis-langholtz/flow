#ifndef prototype_hpp
#define prototype_hpp

#include <map>
#include <string>
#include <variant>
#include <vector>

#include "flow/descriptor_id.hpp"
#include "flow/connection.hpp"
#include "flow/io_type.hpp"
#include "flow/process_name.hpp"

namespace flow {

struct descriptor {
    std::string comment;
    io_type direction;
};

using descriptor_container = std::map<descriptor_id, descriptor>;

inline const auto standard_descriptors = descriptor_container{
    {descriptor_id{0}, {"stdin", io_type::in}},
    {descriptor_id{1}, {"stdout", io_type::out}},
    {descriptor_id{2}, {"stderr", io_type::out}},
};

struct system_prototype;
struct executable_prototype;

using prototype = std::variant<executable_prototype, system_prototype>;

using argument_container = std::vector<std::string>;
using environment_container = std::map<std::string, std::string>;

struct executable_prototype {
    descriptor_container descriptors;
    std::filesystem::path working_directory;
    std::filesystem::path path;
    argument_container arguments;
    environment_container environment;
};

struct system_prototype {
    descriptor_container descriptors;
    std::map<process_name, prototype> prototypes;
    std::vector<connection> connections;
};

}

#endif /* prototype_hpp */
