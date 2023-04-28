#ifndef system_error_hpp
#define system_error_hpp

#include <ostream>
#include <string>

namespace flow {

enum class system_error_code: int;

auto operator<<(std::ostream& os, system_error_code err)
    -> std::ostream&;

auto to_string(system_error_code err) -> std::string;

}

#endif /* system_error_hpp */
