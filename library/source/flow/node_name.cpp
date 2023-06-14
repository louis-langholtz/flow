#include <iomanip> // for std::quoted

#include "flow/node_name.hpp"

namespace flow {

auto operator<<(std::ostream& os, const node_name& name) -> std::ostream&
{
    os << name.get();
    return os;
}

auto to_system_names(std::string_view string,
                     const std::string_view& separator)
    -> std::deque<node_name>
{
    auto result = std::deque<node_name>{};
    auto pos = decltype(string.find(separator)){};
    while ((pos = string.find(separator)) != std::string_view::npos) {
        result.emplace_back(std::string{string.substr(0, pos)});
        string.remove_prefix(pos + 1);
    }
    if (!empty(string) || !empty(result)) {
        result.emplace_back(std::string{string});
    }
    return result;
}

}
