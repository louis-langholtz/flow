#ifndef instance_hpp
#define instance_hpp

#include <map>
#include <ostream>
#include <type_traits> // for std::is_default_constructible_v
#include <vector>

#include "ext/fstream.hpp"

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/environment_map.hpp"
#include "flow/owning_process_id.hpp"
#include "flow/reference_process_id.hpp"
#include "flow/system_name.hpp"
#include "flow/variant.hpp"
#include "flow/wait_status.hpp"

namespace flow {

/// @brief Instance of a <code>system</code>.
struct instance
{
    struct custom
    {
        /// @brief Default constructor.
        /// @note This ensures C++ sees this class as default constructable.
        custom() noexcept {}; // NOLINT(modernize-use-equals-default)

        /// @brief Sub-instances - or children - of this instance.
        std::map<system_name, instance> children;

        /// @brief Channels made for this instance.
        std::vector<channel> channels;

        reference_process_id pgrp{no_process_id};
    };

    struct forked
    {
        /// @brief Diagnostics stream.
        /// @note Make this unique to this instance's process ID to avoid racy
        ///   or disordered output.
        ext::fstream diags;

        variant<owning_process_id, wait_status> state;
    };

    environment_map environment;
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
auto get_reference_process_id(const std::vector<system_name>& names,
                              const instance& object)
    -> reference_process_id;
auto total_descendants(const instance& object) -> std::size_t;
auto total_channels(const instance& object) -> std::size_t;
auto get_wait_status(const instance& object) -> wait_status;

struct system;

auto instantiate(const system_name& name, const system& sys,
                 std::ostream& diags, environment_map env = {})
    -> instance;

}

#endif /* instance_hpp */
