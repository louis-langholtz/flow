#ifndef system_endpoint_hpp
#define system_endpoint_hpp

#include <ostream>
#include <set>
#include <utility> // for std::move

#include "flow/descriptor_id.hpp"
#include "flow/system_name.hpp"

namespace flow {

struct system_endpoint {
    template <class... T>
    system_endpoint(system_name a = {}, T... d)
    : address{std::move(a)}, descriptors{std::move(d)...} {}

    system_name address;

    ///@brief Well known descriptor ID of endpoint for systems.
    std::set<descriptor_id> descriptors;
};

constexpr auto operator==(const system_endpoint& lhs,
                          const system_endpoint& rhs) -> bool
{
    return (lhs.address == rhs.address)
        && (lhs.descriptors == rhs.descriptors);
}

auto operator<<(std::ostream& os, const system_endpoint& value)
    -> std::ostream&;

}

#endif /* system_endpoint_hpp */
