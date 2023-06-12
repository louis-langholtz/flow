#include <stdexcept> // for std::invalid_argument
#include <sstream> // for std::ostringstream

#include "flow/charset_checker.hpp"

namespace flow {

charset_validator_error::charset_validator_error(char c,
                                                 std::string chars,
                                                 char_list acc,
                                                 const std::string& what_arg):
    invalid_argument(what_arg),
    chars_(std::move(chars)), access_(acc), badchar_(c)
{
    // Intentionally empty.
}

auto charset_validator_error::access() const noexcept -> char_list
{
    return access_;
}

auto charset_validator_error::charset() const -> std::string
{
    return chars_;
}

auto charset_validator_error::badchar() const noexcept -> char
{
    return badchar_;
}

}

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
        if (access == char_list::deny) {
            os << ", character denied";
        }
        else {
            os << ", character not allowed";
        }
        throw charset_validator_error{c, chars, access, os.str()};
    }
    return v;
}

}
