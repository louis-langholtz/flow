#ifndef os_error_code_hpp
#define os_error_code_hpp

#include <ostream>
#include <string>

namespace flow {

/// @brief System error code.
/// @note This is an internal library type not intended for direct use by library users.
enum class os_error_code: int;

auto operator<<(std::ostream& os, os_error_code err)
    -> std::ostream&;

auto to_string(os_error_code err) -> std::string;

}

#endif /* os_error_code_hpp */
