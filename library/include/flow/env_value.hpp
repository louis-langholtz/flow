#ifndef env_value_hpp
#define env_value_hpp

#include <string>

#include "flow/checked_value.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct env_value_checker:
    detail::denied_chars_checker<detail::constexpr_ntbs<'\0'>> {};

/// @brief Environment variable value strong type.
/// @note Environment variable values may not contain the null-character ('\0').
///   This character is invalid.
/// @throws std::invalid_argument if an attempt is made to construct this type
///   with one or more invalid characters.
using env_value = detail::checked_value<std::string, env_value_checker>;

static_assert(std::is_nothrow_default_constructible_v<env_value>);
static_assert(std::is_nothrow_move_constructible_v<env_value>);
static_assert(std::is_nothrow_move_assignable_v<env_value>);
static_assert(std::is_copy_constructible_v<env_value>);
static_assert(std::is_copy_assignable_v<env_value>);

}

#endif /* env_value_hpp */
