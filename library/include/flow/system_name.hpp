#ifndef system_name_hpp
#define system_name_hpp

#include <concepts>
#include <deque>
#include <ostream>
#include <string>
#include <string_view>

#include "flow/checked.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct system_name_checker: detail::allowed_chars_checker<detail::name_charset>
{
    static constexpr auto separator = ".";
};

/// @brief System name.
/// @details A lexical token for identifying a <code>system</code>.
/// @note This is a strongly typed <code>std::string</code> that can be
///   constructed from strings containing only characters from its allowed
///   character set. An <code>std::invalid_argument</code> exception is
///   thrown otherwise.
/// @see system.
using system_name = detail::checked<std::string, system_name_checker>;

auto operator<<(std::ostream& os, const system_name& name) -> std::ostream&;

template <class T, class V>
concept is_iterable = requires(T x)
{
    {*begin(x)} -> std::same_as<V&>;
    {*end(x)} -> std::same_as<V&>;
};

template <is_iterable<system_name> T>
auto operator<<(std::ostream& os, const T& names) -> std::ostream&
{
    auto prefix = "";
    for (auto&& name: names) {
        os << prefix << name.get();
        prefix = system_name_checker::separator;
    }
    return os;
}

/// @brief Splits the given string by the specified separator.
/// @throws std::invalid_argument if any component is an invalid
///   <code>system_name</code>.
auto to_system_names(std::string_view string,
                     const std::string_view& separator =
                        system_name_checker::separator)
    -> std::deque<system_name>;

}

#endif /* system_name_hpp */
