#ifndef instantiate_hpp
#define instantiate_hpp

#include <ostream>
#include <stdexcept> // for std::invalid_argument

#include "flow/system.hpp"
#include "flow/instance.hpp"

namespace flow {

/// @brief Options for <code>instantiate</code>.
/// @see instantiate.
struct instantiate_options
{
    /// @brief Available ports.
    /// @note Set to default/empty/none to require closed system.
    port_map ports;

    /// @brief Base environment settings.
    environment_map environment;
};

struct invalid_executable: std::invalid_argument
{
    using std::invalid_argument::invalid_argument;
};

struct invalid_port_map: std::invalid_argument
{
    using std::invalid_argument::invalid_argument;
};

/// @brief Instantiates the given system.
/// @param[in] sys System to attempt to instantiate.
/// @param[out] diags Diagnostic information and warnings that don't by
///   themselves prevent instantiation.
/// @throws invalid_connection if a <code>connection</code> specified by @sys
///   (or any of its sub-systems) is invalid such that a <code>channel</code>
///   cannot be made for it.
/// @throws invalid_executable if a <code>system::executable</code> specified
///   by @sys (or any of its sub-systems) is invalid such that an
///   <code>instance</code> cannot be made for it.
/// @throws invalid_port_map if a <code>port_map</code> specified
///   by @sys (or any of its sub-systems) is invalid such that an
///   <code>instance</code> cannot be made for it.
/// @see instance.
auto instantiate(const system& sys, std::ostream& diags,
                 const instantiate_options& opts = {})
    -> instance;

}

#endif /* instantiate_hpp */
