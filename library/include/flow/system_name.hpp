#ifndef system_name_hpp
#define system_name_hpp

#include <concepts>
#include <deque>
#include <ostream>
#include <string>
#include <string_view>

#include "flow/checked.hpp"
#include "flow/charset_checker.hpp"
#include "flow/reserved.hpp"

namespace flow {

struct node_name_checker: detail::allowed_chars_checker<detail::name_charset>
{
};

/// @brief System name.
/// @details A lexical token for identifying a <code>node</code>.
/// @note This is a strongly typed <code>std::string</code> that can be
///   constructed from strings containing only characters from its allowed
///   character set. An <code>charset_validator_error</code> exception is
///   thrown otherwise.
/// @see system.
using system_name = detail::checked<std::string, node_name_checker>;

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
    auto add_separator = false;
    for (auto&& name: names) {
        if (add_separator) {
            os << reserved::system_name_separator;
        }
        os << name.get();
        add_separator = true;
    }
    return os;
}

/// @brief Splits the given string by the specified separator.
/// @throws charset_validator_error if any component is an invalid
///   <code>system_name</code>.
auto to_system_names(std::string_view string,
                     const std::string_view& separator =
                     std::string{reserved::system_name_separator})
    -> std::deque<system_name>;

}

#endif /* system_name_hpp */
