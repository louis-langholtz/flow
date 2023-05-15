#ifndef env_value_hpp
#define env_value_hpp

#include <ostream>
#include <string>

#include "flow/checked_value.hpp"

namespace flow {

struct env_value_checker
{
    static_assert(std::is_nothrow_default_constructible_v<std::string>);

    auto operator()() const noexcept // NOLINT(bugprone-exception-escape)
        -> std::string
    {
        return {};
    }

    auto operator()(std::string v) const -> std::string;

    auto operator()(const char *v) const -> std::string
    {
        return operator()(std::string(v));
    }

    template <class InputIt>
    auto operator()(InputIt first, InputIt last) const -> std::string
    {
        return operator()(std::string{first, last});
    }
};

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
