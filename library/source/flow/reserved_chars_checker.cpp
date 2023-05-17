#include <stdexcept> // for std::invalid_argument
#include <sstream> // for std::ostringstream

#include "flow/reserved_chars_checker.hpp"

namespace flow::detail {

auto reserved_chars_validator(std::string v,
                              const std::string& reserved_chars)
    -> std::string
{
    if (const auto found = v.find_first_of(reserved_chars);
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
