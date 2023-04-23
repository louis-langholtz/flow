#ifndef process_port_hpp
#define process_port_hpp

#include <ostream>

#include "flow/descriptor_id.hpp"
#include "flow/prototype_name.hpp"

namespace flow {

struct prototype_port {
    prototype_name address;

    ///@brief Well known descriptor ID for port.
    descriptor_id descriptor{invalid_descriptor_id};
};

std::ostream& operator<<(std::ostream& os, const prototype_port& value);

}

#endif /* process_port_hpp */
