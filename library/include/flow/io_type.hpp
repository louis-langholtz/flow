#ifndef io_type_hpp
#define io_type_hpp

#include <ostream>

namespace flow {

enum class io_type: unsigned {
    in = 0x01u, out = 0x02u, bidir = 0x03u};

constexpr auto reverse(io_type io) noexcept -> io_type
{
    switch (io) {
    case io_type::in: return io_type::out;
    case io_type::out: return io_type::in;
    case io_type::bidir: return io_type::bidir;
    }
    return io;
}

constexpr auto to_cstring(io_type io) -> const char*
{
    switch (io) {
    case io_type::in: return "read";
    case io_type::out: return "write";
    case io_type::bidir: return "read+write";
    }
    return "unknown";
}

auto operator<<(std::ostream& os, io_type value) -> std::ostream&;

}

#endif /* io_type_hpp */
