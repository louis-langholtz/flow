#ifndef user_endpoint_hpp
#define user_endpoint_hpp

#include <ostream>

namespace flow {

struct user_endpoint {
    auto operator<=>(const user_endpoint&) const = default;
};

auto operator<<(std::ostream& os, const user_endpoint& value)
    -> std::ostream&;

}

#endif /* user_endpoint_hpp */
