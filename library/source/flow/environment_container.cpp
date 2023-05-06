#include <cctype> // for std::isprint
#include <cstring> // for std::strchr

#include "flow/environment_container.hpp"

extern char **environ; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace flow {

namespace {

constexpr auto env_separator = '=';

}

auto operator<<(std::ostream& os, const environment_container& value)
    -> std::ostream&
{
    os << "environment={\n";
    for (auto&& entry: value) {
        os << "  " << entry.first << env_separator << entry.second << "\n";
    }
    os << "}\n";
    return os;
}

auto get_environ() -> environment_container
{
    environment_container result;
    for (auto env = ::environ; env && *env; ++env) {
        if (const auto found = std::strchr(*env, '=')) {
            result[{*env, found}] = std::string(found + 1);
        }
        else {
            result[*env] = std::string();
        }
    }
    return result;
}

auto make_arg_bufs(const std::map<env_name, std::string>& envars)
    -> std::vector<std::string>
{
    auto result = std::vector<std::string>{};
    for (const auto& entry: envars) {
        auto string = std::string(entry.first);
        string += env_separator;
        string += entry.second;
        result.push_back(std::move(string));
    }
    return result;
}

}
