#ifndef node_name_hpp
#define node_name_hpp

#include <concepts> // for std::regular.
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

/// @brief Node name.
/// @details A lexical token for identifying a <code>node</code>.
/// @note This is a strongly typed <code>std::string</code> that can be
///   constructed from strings containing only characters from its allowed
///   character set. An <code>charset_validator_error</code> exception is
///   thrown otherwise.
/// @see node.
using node_name = detail::checked<std::string, node_name_checker>;

static_assert(std::regular<node_name>);

auto operator<<(std::ostream& os, const node_name& name) -> std::ostream&;

template <class T, class V>
concept is_iterable = requires(T x)
{
    {*begin(x)} -> std::same_as<V&>;
    {*end(x)} -> std::same_as<V&>;
};

template <is_iterable<node_name> T>
auto operator<<(std::ostream& os, const T& names) -> std::ostream&
{
    auto add_separator = false;
    for (auto&& name: names) {
        if (add_separator) {
            os << reserved::node_name_separator;
        }
        os << name.get();
        add_separator = true;
    }
    return os;
}

/// @brief Splits the given string by the specified separator.
/// @throws charset_validator_error if any component is an invalid
///   <code>node_name</code>.
auto to_node_names(std::string_view string,
                   const std::string_view& separator =
                   std::string{reserved::node_name_separator})
    -> std::deque<node_name>;

}

#endif /* node_name_hpp */
