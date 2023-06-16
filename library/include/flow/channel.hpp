#ifndef channel_hpp
#define channel_hpp

#include <ostream>
#include <span>
#include <stdexcept> // for std::invalid_argument
#include <type_traits> // for std::is_default_constructible_v

#include "flow/link.hpp"
#include "flow/file_channel.hpp"
#include "flow/forwarding_channel.hpp"
#include "flow/pipe_channel.hpp"
#include "flow/signal_channel.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct node;
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

struct invalid_link: std::invalid_argument
{
    invalid_link(link l, const std::string& what_arg):
        std::invalid_argument(what_arg), value(std::move(l))
    {}
    invalid_link(link l, const char* what_arg):
        std::invalid_argument(what_arg), value(std::move(l))
    {}

    link value;
};

/// @brief Makes a <code>channel</code> for a <code>link</code>.
/// @throws invalid_link if something is invalid about
///   @link for the given context that prevents making the
///   <code>channel</code>.
/// @throws std::logic_error if size of @parent_links doesn't match
///   size of @parent_channels.
/// @see channel.
auto make_channel(const link& for_link,
                  const node_name& name,
                  const node& node,
                  const std::span<channel>& channels,
                  const std::span<const link>& parent_links,
                  const std::span<channel>& parent_channels)
    -> channel;

auto operator<<(std::ostream& os, const reference_channel& value)
    -> std::ostream&;

}

#endif /* channel_hpp */
