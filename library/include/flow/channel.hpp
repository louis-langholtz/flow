#ifndef channel_hpp
#define channel_hpp

#include <ostream>
#include <span>
#include <type_traits>

#include "flow/connection.hpp"
#include "flow/file_channel.hpp"
#include "flow/pipe_channel.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct system_name;
struct system;

struct reference_channel
{
    using channel = std::variant<
        file_channel,
        pipe_channel,
        reference_channel
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
