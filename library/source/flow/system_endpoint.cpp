#include <algorithm>
#include <charconv> // for std::from_chars
#include <iomanip> // for std::quoted
#include <sstream> // for std::ostringstream
#include <stdexcept> // for std::invalid_argument
#include <system_error>

#include "flow/system_endpoint.hpp"

namespace flow {
namespace {
const auto descriptor_separator = ",";
}

auto operator<<(std::ostream& os, const system_endpoint& value) -> std::ostream&
{
    using std::begin, std::end;
    // ensure separator not valid char for address
    static_assert(std::count(begin(detail::name_charset{}),
                             end(detail::name_charset{}),
                             system_endpoint::separator) == 0);
    os << "system_endpoint{";
    os << value.address.get();
    os << system_endpoint::separator;
    auto prefix = "";
    for (auto&& descriptor: value.descriptors) {
        os << prefix << descriptor;
        prefix = descriptor_separator;
    }
    os << "}";
    return os;
}

auto to_descriptors(std::string_view string) -> std::set<reference_descriptor>
{
    auto dset = std::set<reference_descriptor>{};
    auto pos = decltype(string.find(descriptor_separator)){};
    while ((pos = string.find(descriptor_separator)) !=
           std::string_view::npos) {
        const auto comp = string.substr(0, pos);
        string.remove_prefix(pos + 1);
        auto integer = 0;
        const auto [ptr, ec] = std::from_chars(data(comp),
                                               data(comp) + size(comp),
                                               integer);
        if ((ptr != (data(comp) + size(comp))) || (ec != std::errc())) {
            std::ostringstream os;
            switch (ec) {
            case std::errc::invalid_argument:
                os << std::quoted(comp) << " not a number.\n";
                break;
            case std::errc::result_out_of_range:
                os << std::quoted(comp) << " larger than an int.\n";
                break;
            default:
                os << std::quoted(comp) << ": error " << int(ec) << "\n";
                break;
            }
            throw std::invalid_argument{os.str()};
        }
        dset.insert(reference_descriptor{integer});
    }
    return dset;
}

auto to_system_endpoint(std::string_view string, char separator)
    -> system_endpoint
{
    auto pos = string.find(separator);
    const auto address_str = string.substr(0, pos);
    if (pos != std::string_view::npos) {
        string.remove_prefix(pos + 1);
    }
    else {
        string = std::string_view{};
    }
    return {system_name{std::string{address_str}}, to_descriptors(string)};
}

}
