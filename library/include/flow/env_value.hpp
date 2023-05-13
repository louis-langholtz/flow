#ifndef env_value_hpp
#define env_value_hpp

#include <ostream>
#include <string>

#include "flow/checked_value.hpp"

namespace flow {

struct env_value_checker
{
    auto operator()() const -> std::string
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

using env_value = detail::checked_value<std::string, env_value_checker>;

static_assert(std::is_default_constructible_v<env_value>);
static_assert(std::is_copy_constructible_v<env_value>);
static_assert(std::is_move_constructible_v<env_value>);
static_assert(std::is_copy_assignable_v<env_value>);
static_assert(std::is_move_assignable_v<env_value>);

}

#endif /* env_value_hpp */
