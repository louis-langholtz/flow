#ifndef process_id_hpp
#define process_id_hpp

#include <ostream>

#include "flow/owning_process_id.hpp"
#include "flow/reference_process_id.hpp"
#include "flow/variant.hpp"

namespace flow {

using process_id = variant<reference_process_id, owning_process_id>;

inline auto to_reference_process_id(const process_id& pid)
    -> reference_process_id
{
    if (const auto p = std::get_if<owning_process_id>(&pid)) {
        return reference_process_id(*p);
    }
    if (const auto p = std::get_if<reference_process_id>(&pid)) {
        return *p;
    }
    return invalid_process_id;
}

inline auto operator==(const process_id& var, const reference_process_id& ref)
    -> bool
{
    return to_reference_process_id(var) == ref;
}

inline auto operator==(const reference_process_id& ref, const process_id& var)
    -> bool
{
    return to_reference_process_id(var) == ref;
}

}

#endif /* process_id_hpp */
