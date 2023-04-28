#ifndef instance_hpp
#define instance_hpp

#include <map>
#include <ostream>
#include <vector>

#include "ext/fstream.hpp"

#include "flow/channel.hpp"
#include "flow/process_id.hpp"
#include "flow/prototype_name.hpp"

namespace flow {

struct instance {
    process_id id{invalid_process_id};
    ext::fstream diags;
    std::map<prototype_name, instance> children;
    std::vector<channel> channels;
};

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&;

struct system_prototype;

auto instantiate(const system_prototype& system,
                 std::ostream& err_stream) -> instance;

}

#endif /* instance_hpp */
