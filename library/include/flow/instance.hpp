#ifndef instance_hpp
#define instance_hpp

#include <map>
#include <ostream>
#include <type_traits> // for std::is_default_constructible_v
#include <vector>

#include "ext/fstream.hpp"

#include "flow/channel.hpp"
#include "flow/link.hpp"
#include "flow/environment_map.hpp"
#include "flow/owning_process_id.hpp"
#include "flow/reference_process_id.hpp"
#include "flow/node_name.hpp"
#include "flow/variant.hpp"
#include "flow/wait_status.hpp"

namespace flow {

/// @brief Instance of a <code>node</code>.
/// @note This type is intended to be moveable, but not copyable.
///   Relatedly, it's not intended to support any kind of comparison.
///   This alleviates all component types from having to be copyable or
///   having to support comparison.
/// @see node.
struct instance
{
    /// @brief Information specific to "system" instances.
    /// @note Instantiating a system node, results in a system instance.
    /// @see flow::system.
    struct custom
    {
        static constexpr auto default_pgrp = no_process_id;

        /// @brief Default constructor.
        /// @note This ensures compilers see class as default constructable.
        custom() noexcept = default;

        /// @brief Sub-instances - or children - of this instance.
        std::map<node_name, instance> children;

        /// @brief Channels made for this instance.
        std::vector<channel> channels;

        reference_process_id pgrp{default_pgrp};
    };

    /// @brief Information specific to "forked" instances.
    /// @note Instantiating an executable node, results in a forked instance.
    /// @see executable.
    struct forked
    {
        /// @brief Diagnostics stream.
        /// @note Make this unique to this instance's process ID to avoid racy
        ///   or disordered output.
        ext::fstream diags;

        variant<owning_process_id, wait_status> state;
    };

    variant<custom, forked> info;
};

static_assert(std::is_default_constructible_v<instance::custom>);
static_assert(std::is_move_constructible_v<instance::custom>);
static_assert(std::is_move_assignable_v<instance::custom>);

static_assert(std::is_default_constructible_v<instance::forked>);

static_assert(std::is_default_constructible_v<instance>);
static_assert(std::is_move_constructible_v<instance>);
static_assert(std::is_move_assignable_v<instance>);

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&;

auto pretty_print(std::ostream& os, const instance& value) -> void;

auto get_reference_process_id(const instance::forked& object)
    -> reference_process_id;
auto get_reference_process_id(const std::vector<node_name>& names,
                              const instance& object)
    -> reference_process_id;
auto total_descendants(const instance& object) -> std::size_t;
auto total_channels(const instance& object) -> std::size_t;
auto get_wait_status(const instance& object) -> wait_status;

}

#endif /* instance_hpp */
