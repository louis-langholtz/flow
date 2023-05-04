#include <cstring> // for std::strchr

#include "flow/environment_container.hpp"

extern char **environ; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace flow {

auto get_environ() -> environment_container
{
    environment_container result;
    for (auto env = ::environ; env && *env; ++env) {
        if (const auto found = std::strchr(*env, '=')) {
            result[std::string(*env, found)] = std::string(found + 1);
        }
        else {
            result[std::string(*env)] = std::string();
        }
    }
    return result;
}

}
