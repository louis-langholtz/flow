#ifndef instance_hpp
#define instance_hpp

#include <map>
#include <ostream>
#include <vector>

#include "ext/fstream.hpp"

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/process_id.hpp"
#include "flow/prototype_name.hpp"

namespace flow {

struct instance {

    /// @brief Process ID.
    /// @note This shall be <code>invalid_process_id</code> for default or
    /// failed instances, <code>no_process_id</code> for system instances,
    /// less-than <code>no_process_id</code> for the process group of the
    /// children, or hold the POSIX process ID of the child process running
    /// for the instance.
    process_id id{invalid_process_id};

    /// @brief Diagnostics stream.
    /// @note This should be unique to this instance's process ID.
    ext::fstream diags;

    /// @brief Sub-instances - or children - of this instance.
    std::map<prototype_name, instance> children;

    /// @brief Channels made for this instance.
    std::vector<channel> channels;
};

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&;

auto pretty_print(std::ostream& os, const instance& value) -> void;

auto total_descendants(const instance& object) -> std::size_t;
auto total_channels(const instance& object) -> std::size_t;

struct system_prototype;

auto instantiate(const prototype_name& name, const system_prototype& system,
                 std::ostream& diags,
                 const std::span<const connection>& parent_connections,
                 const std::span<channel>& parent_channels) -> instance;

}

#endif /* instance_hpp */
