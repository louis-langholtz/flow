#ifndef system_name_hpp
#define system_name_hpp

#include <ostream>
#include <string>
#include <vector>

#include "flow/checked_value.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct system_name_checker: public detail::name_chars_checker
{
    static constexpr auto separator = ".";
};

using system_name = detail::checked_value<std::string, system_name_checker>;

auto operator<<(std::ostream& os, const system_name& name) -> std::ostream&;

auto operator<<(std::ostream& os,
                const std::vector<system_name>& names) -> std::ostream&;

}

#endif /* system_name_hpp */
