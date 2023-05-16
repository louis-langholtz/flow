#ifndef instantiate_hpp
#define instantiate_hpp

#include <ostream>

#include "flow/system.hpp"
#include "flow/instance.hpp"

namespace flow {

/// @brief Options for <code>instantiate</code>.
/// @see instantiate.
struct instantiate_options
{
    /// @brief Available descriptors.
    /// @note Set to default/empty/none to require closed system.
    descriptor_map descriptors;

    /// @brief Base environment settings.
    environment_map environment;
};

/// @brief Instantiates the given system.
/// @param[in] sys System to attempt to instantiate.
/// @param[out] diags Diagnostic information and warnings that don't by
///   themselves prevent instantiation.
/// @throws std::invalid_argument if anything about <code>sys</code> is found
///   to be invalid for instantiation.
auto instantiate(const system& sys, std::ostream& diags,
                 const instantiate_options& opts = {})
    -> instance;

}

#endif /* instantiate_hpp */
