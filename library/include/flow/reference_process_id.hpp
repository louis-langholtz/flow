#ifndef reference_process_id_hpp
#define reference_process_id_hpp

#include <cstdint> // for std::int32_t
#include <ostream>
#include <type_traits> // for std::underlying_type_t

namespace flow {

/// @brief Process ID strong type wrapping a POSIX process ID.
enum class reference_process_id: std::int32_t;

constexpr auto invalid_process_id = reference_process_id{
    static_cast<std::underlying_type_t<reference_process_id>>(-1)
};

constexpr auto no_process_id = reference_process_id{
    static_cast<std::underlying_type_t<reference_process_id>>(0)
};

auto current_process_id() noexcept -> reference_process_id;

auto operator<<(std::ostream& os, reference_process_id value)
    -> std::ostream&;

}

#endif /* reference_process_id_hpp */
