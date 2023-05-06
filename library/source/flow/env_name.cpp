#include <sstream> // for std::ostringstream
#include <stdexcept> // for std::invalid_argument

#include "flow/env_name.hpp"

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
    os << std::string(name);
    return os;
}

}
