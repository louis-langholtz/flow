#ifndef environment_container_hpp
#define environment_container_hpp

#include <map>
#include <string>

namespace flow {

using environment_container = std::map<std::string, std::string>;

auto get_environ() -> environment_container;

}

#endif /* environment_container_hpp */
