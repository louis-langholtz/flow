#ifndef env_value_hpp
#define env_value_hpp

#include <concepts> // for std::regular.
#include <string>

#include "flow/checked.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct env_value_checker:
    detail::denied_chars_checker<detail::tcstring<'\0'>> {};

/// @brief Environment variable value strong type.
/// @note Environment variable values may not contain the null-character ('\0').
///   This character is invalid.
/// @throws charset_validator_error if an attempt is made to construct this
///   type with one or more invalid characters.
using env_value = detail::checked<std::string, env_value_checker>;

static_assert(std::regular<env_value>);

}

#endif /* env_value_hpp */
