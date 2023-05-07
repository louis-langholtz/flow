#include <sstream> // for std::ostringstream

#include "flow/env_value.hpp"

namespace flow {

namespace {

constexpr auto nul_character = '\0';

const std::string reserved_set{std::initializer_list<char>{nul_character}};

}

auto env_value_checker::operator()(std::string v) const -> std::string
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

}
