#ifndef pipe_connection_hpp
#define pipe_connection_hpp

#include <ostream>

#include "flow/process_port.hpp"

namespace flow {

/// @note Results in a <code>pipe_channel</code>.
struct pipe_connection {
    process_port in;
    process_port out;
};

std::ostream& operator<<(std::ostream& os, const pipe_connection& value);

}

#endif /* pipe_connection_hpp */
