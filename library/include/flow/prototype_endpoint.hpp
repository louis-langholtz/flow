#ifndef process_endpoint_hpp
#define process_endpoint_hpp

#include <ostream>

#include "flow/descriptor_id.hpp"
#include "flow/system_name.hpp"

namespace flow {

struct prototype_endpoint {
    system_name address;

    ///@brief Well known descriptor ID of endpoint for applicable prototypes.
    descriptor_id descriptor{invalid_descriptor_id};
};

constexpr auto operator==(const prototype_endpoint& lhs,
                          const prototype_endpoint& rhs) -> bool
{
    return (lhs.address == rhs.address) && (lhs.descriptor == rhs.descriptor);
}

auto operator<<(std::ostream& os, const prototype_endpoint& value)
    -> std::ostream&;

}

#endif /* process_endpoint_hpp */
