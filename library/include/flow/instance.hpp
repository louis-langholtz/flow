#ifndef instance_hpp
#define instance_hpp

#include <map>
#include <ostream>
#include <vector>

#include "ext/fstream.hpp"

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/process_id.hpp"
#include "flow/system_name.hpp"
#include "flow/utility.hpp"

namespace flow {

/// @brief Instance of a <code>system</code>.
struct instance {

    /// @brief Process ID.
    /// @note This shall be <code>invalid_process_id</code> for default or
    /// failed instances, <code>no_process_id</code> for system instances,
    /// less-than <code>no_process_id</code> for the process group of the
    /// children, or hold the POSIX process ID of the child process running
    /// for the instance.
    process_id id{invalid_process_id};

    /// @brief Diagnostics stream.
    /// @note Make this unique to this instance's process ID to avoid racy
    ///   or disordered output.
    ext::fstream diags{nulldev_fstream()};

    /// @brief Sub-instances - or children - of this instance.
    std::map<system_name, instance> children{};

    /// @brief Channels made for this instance.
    std::vector<channel> channels{};
};

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&;

auto pretty_print(std::ostream& os, const instance& value) -> void;

auto total_descendants(const instance& object) -> std::size_t;
auto total_channels(const instance& object) -> std::size_t;

struct custom_system;

auto instantiate(const system_name& name, const custom_system& system,
                 std::ostream& diags,
                 const std::span<const connection>& parent_connections,
                 const std::span<channel>& parent_channels) -> instance;

}

#endif /* instance_hpp */
