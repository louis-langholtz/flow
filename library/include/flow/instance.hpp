#ifndef instance_hpp
#define instance_hpp

#include <map>
#include <ostream>
#include <vector>

#include "flow/channel.hpp"
#include "flow/process_id.hpp"
#include "flow/prototype_name.hpp"

namespace flow {

struct instance {
    process_id id;
    std::map<prototype_name, instance> children;
    std::vector<channel> channels;
};

std::ostream& operator<<(std::ostream& os, const instance& value);

}

#endif /* instance_hpp */
