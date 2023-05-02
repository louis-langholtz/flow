#ifndef system_endpoint_hpp
#define system_endpoint_hpp

#include <ostream>

#include "flow/descriptor_id.hpp"
#include "flow/system_name.hpp"

namespace flow {

struct system_endpoint {
    system_name address;

    ///@brief Well known descriptor ID of endpoint for systems.
    descriptor_id descriptor{invalid_descriptor_id};
};

constexpr auto operator==(const system_endpoint& lhs,
                          const system_endpoint& rhs) -> bool
{
    return (lhs.address == rhs.address) && (lhs.descriptor == rhs.descriptor);
}

auto operator<<(std::ostream& os, const system_endpoint& value)
    -> std::ostream&;

}

#endif /* system_endpoint_hpp */
