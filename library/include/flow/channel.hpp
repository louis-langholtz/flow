#ifndef channel_hpp
#define channel_hpp

#include <ostream>
#include <span>
#include <stdexcept> // for std::invalid_argument
#include <type_traits> // for std::is_default_constructible_v

#include "flow/connection.hpp"
#include "flow/file_channel.hpp"
#include "flow/forwarding_channel.hpp"
#include "flow/pipe_channel.hpp"
#include "flow/signal_channel.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct system;
struct instance;

struct reference_channel
{
    using channel = variant<
        reference_channel,
        file_channel,
        pipe_channel,
        signal_channel,
        forwarding_channel
    >;

    /// @brief Non-owning pointer to referenced channel.
    channel *other{};

    auto operator<=>(const reference_channel&) const = default;
};

/// @brief Channel type.
/// @note This is aliased to <code>reference_channel</code>'s
/// <code>channel</code> alias to keep all the possible types set in only
/// one place and to ensure <code>reference_channel</code> is complete by
/// this point.
using channel = reference_channel::channel;

// Confirm that channel is default constructable as it's expected to be.
// This fails unless all of channel's types are complete.
static_assert(std::is_default_constructible_v<channel>);

struct invalid_connection: std::invalid_argument
{
    using std::invalid_argument::invalid_argument;
};

/// @brief Makes a <code>channel</code> for a <code>connection</code>.
/// @throws invalid_connection if something is invalid about
///   @connection for the given context that prevents making the
///   <code>channel</code>.
/// @throws std::logic_error if size of @parent_connections doesn't match
///   size of @parent_channels.
/// @see channel.
auto make_channel(const connection& conn,
                  const system_name& name,
                  const system& system,
                  const std::span<channel>& channels,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> channel;

auto operator<<(std::ostream& os, const reference_channel& value)
    -> std::ostream&;

}

#endif /* channel_hpp */
