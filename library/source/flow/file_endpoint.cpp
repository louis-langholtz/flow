#include <iomanip> // for std::quoted

#include "flow/file_endpoint.hpp"
#include "flow/reserved.hpp"

namespace flow {

const file_endpoint file_endpoint::dev_null = file_endpoint{"/dev/null"};

auto operator<<(std::ostream& os, const file_endpoint& value) -> std::ostream&
{
    os << reserved::file_endpoint_prefix;
    // Uses std::quoted to ensure consistency with input
    os << std::quoted(value.path.string());
    return os;
}

auto operator>>(std::istream& is, file_endpoint& value) -> std::istream&
{
    if (is.peek() != reserved::file_endpoint_prefix) {
        is.setstate(std::ios::failbit);
        return is;
    }
    auto c = char{};
    is >> c; // skip the prefix char
    auto string = std::string{};
    is >> std::quoted(string);
    value = file_endpoint{string};
    return is;
}

}
