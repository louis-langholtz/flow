#include "flow/file_endpoint.hpp"

namespace flow {

namespace {
constexpr auto prefix_char = '"';
}

const file_endpoint file_endpoint::dev_null = file_endpoint{"/dev/null"};

auto operator<<(std::ostream& os, const file_endpoint& value) -> std::ostream&
{
    // Uses std::quoted to ensure consistency with input
    os << std::quoted(value.path.string());
    return os;
}

auto operator>>(std::istream& is, file_endpoint& value) -> std::istream&
{
    if (is.peek() != prefix_char) {
        is.setstate(std::ios::failbit);
        return is;
    }
    auto string = std::string{};
    is >> std::quoted(string);
    value = file_endpoint{string};
    return is;
}

}
