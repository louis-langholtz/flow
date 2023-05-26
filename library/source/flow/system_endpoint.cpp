#include <algorithm>
#include <charconv> // for std::from_chars
#include <iomanip> // for std::quoted
#include <sstream> // for std::ostringstream
#include <stdexcept> // for std::invalid_argument
#include <system_error>

#include "flow/reserved.hpp"
#include "flow/system_endpoint.hpp"

namespace flow {

namespace {

auto abs_sub(std::string::size_type lhs, std::string::size_type rhs)
    -> std::string::size_type
{
    const auto npos = std::string::npos;
    if (lhs < rhs) {
        return npos;
    }
    if (lhs == npos) {
        return npos;
    }
    return lhs - rhs;
}

}

auto operator<<(std::ostream& os, const system_endpoint& value) -> std::ostream&
{
    using std::begin, std::end;
    // ensure separator not valid char for address
    static_assert(std::count(begin(detail::name_charset{}),
                             end(detail::name_charset{}),
                             reserved::descriptors_prefix) == 0);
    const auto empty_address = empty(value.address.get());
    const auto empty_descriptors = empty(value.descriptors);
    if (!empty_descriptors) {
        os << reserved::descriptors_prefix;
        auto need_separator = false;
        for (auto&& descriptor: value.descriptors) {
            if (need_separator) {
                os << reserved::descriptor_separator;
            }
            need_separator = true;
            os << descriptor;
        }
    }
    if (!empty_address) {
        os << reserved::address_prefix;
        os << value.address.get();
    }
    if (empty_address && empty_descriptors) {
        // output something so identifiable still as system_endpoint
        os << reserved::descriptors_prefix;
    }
    return os;
}

auto operator>>(std::istream& is, system_endpoint& value) -> std::istream&
{
    const auto first_char = is.peek();
    if ((first_char != reserved::address_prefix) &&
        (first_char != reserved::descriptors_prefix)) {
        is.setstate(std::ios::failbit);
        return is;
    }
    auto string = std::string{};
    is >> string;
    const auto npos = std::string::npos;
    const auto apos = string.find(reserved::address_prefix);
    const auto dpos = string.find(reserved::descriptors_prefix);
    const auto address = (apos != npos)?
        string.substr(apos + 1u, abs_sub(dpos, apos + 1u)): std::string{};
    const auto descriptors = (dpos != npos)?
        string.substr(dpos + 1u, abs_sub(apos, dpos + 1u)): std::string{};
    value = system_endpoint{system_name{address}, to_descriptors(descriptors)};
    return is;
}

auto to_descriptors(std::string_view string) -> std::set<reference_descriptor>
{
    auto get_integer = [](const std::string_view& comp){
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
        return integer;
    };
    auto dset = std::set<reference_descriptor>{};
    auto pos = decltype(string.find(reserved::descriptor_separator)){};
    while ((pos = string.find(reserved::descriptor_separator)) !=
           std::string_view::npos) {
        const auto comp = string.substr(0, pos);
        string.remove_prefix(pos + 1);
        dset.insert(reference_descriptor{get_integer(comp)});
    }
    if (!empty(string)) {
        dset.insert(reference_descriptor{get_integer(string)});
    }
    return dset;
}

}
