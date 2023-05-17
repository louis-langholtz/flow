#ifndef environment_map_hpp
#define environment_map_hpp

#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "flow/env_name.hpp"
#include "flow/env_value.hpp"

namespace flow {

using environment_map = std::map<env_name, env_value>;

auto operator<<(std::ostream& os, const environment_map& value)
    -> std::ostream&;

auto pretty_print(std::ostream& os, const environment_map& value,
                  const std::string& sep = ",\n") -> void;

auto get_environ() -> environment_map;

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable
/// for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
auto make_arg_bufs(const environment_map& envars)
    -> std::vector<std::string>;

}

#endif /* environment_map_hpp */
