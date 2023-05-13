#ifndef env_name_hpp
#define env_name_hpp

#include <ostream>
#include <string>

#include "flow/checked_value.hpp"

namespace flow {

struct env_name_checker
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

using env_name = detail::checked_value<std::string, env_name_checker>;

}

#endif /* env_name_hpp */
