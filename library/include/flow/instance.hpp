#ifndef instance_hpp
#define instance_hpp

#include <map>
#include <ostream>
#include <vector>

#include "flow/channel.hpp"
#include "flow/fstream.hpp"
#include "flow/process_id.hpp"
#include "flow/prototype_name.hpp"

namespace flow {

struct instance {
    process_id id;
    fstream diags;
    std::map<prototype_name, instance> children;
    std::vector<channel> channels;
};

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&;

auto temporary_fstream() -> fstream;

}

#endif /* instance_hpp */
