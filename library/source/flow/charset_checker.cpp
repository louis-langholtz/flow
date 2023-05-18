#include <stdexcept> // for std::invalid_argument
#include <sstream> // for std::ostringstream

#include "flow/charset_checker.hpp"

namespace flow::detail {

auto charset_validator(std::string v,
                       char_list access,
                       const std::string& chars)
    -> std::string
{
    const auto found = (access == char_list::deny)
        ? v.find_first_of(chars)
        : v.find_first_not_of(chars);
    if (found != std::string::npos) {
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
