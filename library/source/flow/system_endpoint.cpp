#include <algorithm>

#include "flow/system_endpoint.hpp"

namespace flow {

auto operator<<(std::ostream& os, const system_endpoint& value) -> std::ostream&
{
    using std::begin, std::end;
    constexpr auto separator = ':';
    // ensure separator not valid char for address
    static_assert(std::count(begin(detail::name_charset{}),
                             end(detail::name_charset{}),
                             separator) == 0);
    os << "system_endpoint{";
    os << value.address.get();
    os << separator;
    auto prefix = "";
    for (auto&& descriptor: value.descriptors) {
        os << prefix << descriptor;
        prefix = ",";
    }
    os << "}";
    return os;
}

}
