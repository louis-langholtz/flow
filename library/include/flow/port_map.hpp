#ifndef port_map_hpp
#define port_map_hpp

#include <map>
#include <ostream>

#include "flow/reference_descriptor.hpp"
#include "flow/port_info.hpp"

namespace flow {

using port_map = std::map<reference_descriptor, port_info>;
using port_map_entry = port_map::value_type;

auto operator<<(std::ostream& os,
                const port_map& value)
-> std::ostream&;

const auto stdin_descriptors_entry = port_map_entry{
    reference_descriptor{0}, {"stdin", io_type::in}
};

const auto stdout_descriptors_entry = port_map_entry{
    reference_descriptor{1}, {"stdout", io_type::out}
};

const auto stderr_descriptors_entry = port_map_entry{
    reference_descriptor{2}, {"stderr", io_type::out}
};

const auto std_descriptors = port_map{
    stdin_descriptors_entry,
    stdout_descriptors_entry,
    stderr_descriptors_entry,
};

}

#endif /* port_map_hpp */
