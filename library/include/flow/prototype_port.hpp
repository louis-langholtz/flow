#ifndef process_port_hpp
#define process_port_hpp

#include <ostream>

#include "flow/descriptor_id.hpp"
#include "flow/prototype_name.hpp"

namespace flow {

struct prototype_port {
    prototype_name address;

    ///@brief Well known descriptor ID of endpoint for applicable prototypes.
    descriptor_id descriptor{invalid_descriptor_id};
};

constexpr auto operator==(const prototype_port& lhs,
                          const prototype_port& rhs) -> bool
{
    return (lhs.address == rhs.address) && (lhs.descriptor == rhs.descriptor);
}

auto operator<<(std::ostream& os, const prototype_port& value)
    -> std::ostream&;

}

#endif /* process_port_hpp */
