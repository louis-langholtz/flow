#ifndef channel_hpp
#define channel_hpp

#include <ostream>
#include <span>

#include "flow/connection.hpp"
#include "flow/io_type.hpp"
#include "flow/pipe_channel.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct system_name;
struct custom_system;

/// @brief File channel.
/// @note This is intended to be a strong place-holder/tag type.
/// @note Instances of this type are made for connections with
///   <code>file_endpoint</code> ends.
/// @see file_endpoint.
struct file_channel {
    io_type io{};
};

auto operator<<(std::ostream& os, const file_channel& value) -> std::ostream&;

struct reference_channel;

using channel = variant<file_channel, pipe_channel, reference_channel>;

struct reference_channel {
    /// @brief Non-owning pointer to referenced channel.
    channel *other{};
};

auto make_channel(const system_name& name, const custom_system& system,
                  const connection& conn,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> channel;

auto operator<<(std::ostream& os, const reference_channel& value)
    -> std::ostream&;

}

#endif /* channel_hpp */
