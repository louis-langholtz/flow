#ifndef system_name_hpp
#define system_name_hpp

#include <ostream>
#include <string>
#include <vector>

#include "flow/checked.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct system_name_checker: detail::allowed_chars_checker<detail::name_charset>
{
    static constexpr auto separator = ".";
};

using system_name = detail::checked<std::string, system_name_checker>;

auto operator<<(std::ostream& os, const system_name& name) -> std::ostream&;

auto operator<<(std::ostream& os,
                const std::vector<system_name>& names) -> std::ostream&;

}

#endif /* system_name_hpp */
