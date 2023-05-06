#ifndef environment_container_hpp
#define environment_container_hpp

#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "flow/env_name.hpp"

namespace flow {

using environment_container = std::map<env_name, std::string>;

auto operator<<(std::ostream& os, const environment_container& value)
    -> std::ostream&;

auto get_environ() -> environment_container;

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable
/// for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
auto make_arg_bufs(const std::map<env_name, std::string>& envars)
    -> std::vector<std::string>;

}

#endif /* environment_container_hpp */
