#ifndef user_endpoint_hpp
#define user_endpoint_hpp

#include <ostream>

namespace flow {

struct user_endpoint {
};

constexpr auto operator==(const user_endpoint& /*lhs*/,
                          const user_endpoint& /*rhs*/) -> bool
{
    return true;
}

auto operator<<(std::ostream& os, const user_endpoint& value)
    -> std::ostream&;

}

#endif /* user_endpoint_hpp */
