#include <cctype> // for std::isprint
#include <cstring> // for std::strchr
#include <sstream> // for std::ostringstream
#include <stdexcept> // for std::invalid_argument

#include "flow/environment_container.hpp"

extern char **environ; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace flow {
namespace {

constexpr auto nul_character = '\0';
constexpr auto env_separator = '=';

}

const std::string env_name::reserved_set{{nul_character, env_separator}};

std::string env_name::validate(std::string v)
{
    if (const auto found = v.find_first_of(reserved_set);
        found != std::string::npos) {
        const auto c = v[found];
        std::ostringstream os;
        os << "may not contain '";
        if (std::isprint(c)) {
            os << c;
        }
        else {
            os << "\\" << std::oct << int(c);
        }
        os << "'";
        throw std::invalid_argument{os.str()};
    }
    return v;
}

auto operator<<(std::ostream& os, const env_name& name)
    -> std::ostream&
{
    os << name.value();
    return os;
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
        auto string = entry.first.value();
        string += env_separator;
        string += entry.second;
        result.push_back(std::move(string));
    }
    return result;
}

}
