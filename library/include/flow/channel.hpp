#ifndef channel_hpp
#define channel_hpp

#include <ostream>
#include <span>

#include "flow/connection.hpp"
#include "flow/file_channel.hpp"
#include "flow/pipe_channel.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct system_name;
struct system;

struct reference_channel;

using channel = variant<file_channel, pipe_channel, reference_channel>;

struct reference_channel {
    /// @brief Non-owning pointer to referenced channel.
    channel *other{};
    auto operator<=>(const reference_channel&) const = default;
};

auto make_channel(const system_name& name,
                  const system& system,
                  const connection& conn,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> channel;

auto operator<<(std::ostream& os, const reference_channel& value)
    -> std::ostream&;

}

#endif /* channel_hpp */
