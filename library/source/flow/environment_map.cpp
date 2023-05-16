#include <cctype> // for std::isprint
#include <cstring> // for std::strchr

#include "flow/environment_map.hpp"

extern char **environ; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace flow {

namespace {

constexpr auto env_separator = '=';

}

auto operator<<(std::ostream& os, const environment_map& value)
    -> std::ostream&
{
    os << "{";
    auto prefix = "";
    for (auto&& entry: value) {
        os << prefix << entry.first << env_separator << entry.second;
        prefix = ",";
    }
    os << "}";
    return os;
}

auto pretty_print(std::ostream& os, const environment_map& value,
                  const std::string& sep) -> void
{
    for (auto&& entry: value) {
        os << entry.first << env_separator << entry.second << sep;
    }
}

auto get_environ() -> environment_map
{
    environment_map result;
    for (auto env = ::environ; env && *env; ++env) {
        const auto found = std::strchr(*env, '=');
        const auto name = found? env_name{*env, found}: env_name{*env};
        const auto value = found? env_value{found + 1}: env_value{};
        result[name] = value;
    }
    return result;
}

auto make_arg_bufs(const environment_map& envars)
    -> std::vector<std::string>
{
    auto result = std::vector<std::string>{};
    for (const auto& entry: envars) {
        auto string = std::string(entry.first);
        string += env_separator;
        string += std::string(entry.second);
        result.push_back(std::move(string));
    }
    return result;
}

}
