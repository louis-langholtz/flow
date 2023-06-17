#ifndef env_name_hpp
#define env_name_hpp

#include <concepts> // for std::regular.
#include <string>

#include "flow/checked.hpp"
#include "flow/charset_checker.hpp"

namespace flow {

struct env_name_checker:
    detail::denied_chars_checker<detail::tcstring<'\0','='>> {};

/// @brief Environment variable name strong type.
/// @note Environment variable names may not contain the null-character ('\0')
///   nor the equals-character ('='). These characters are invalid.
/// @throws charset_validator_error if an attempt is made to construct this
///   type with one or more invalid characters.
using env_name = detail::checked<std::string, env_name_checker>;

static_assert(std::regular<env_name>);

}

#endif /* env_name_hpp */
