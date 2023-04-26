#ifndef io_type_hpp
#define io_type_hpp

#include <ostream>

namespace flow {

enum class io_type: int { in = 0, out = 1};

constexpr auto to_cstring(io_type direction) -> const char*
{
    switch (direction) {
    case io_type::in: return "in";
    case io_type::out: return "out";
    }
    return "";
}

int to_open_flags(io_type direction);

std::ostream& operator<<(std::ostream& os, io_type value);

}

#endif /* io_type_hpp */
