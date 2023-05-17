#ifndef io_type_hpp
#define io_type_hpp

#include <optional>
#include <ostream>
#include <string_view>

namespace flow {

enum class io_type: unsigned {
    none = 0x00u,
    in = 0x01u,
    out = 0x02u,
    bidir = 0x03u,
};

constexpr auto reverse(io_type io) noexcept -> io_type
{
    switch (io) {
    case io_type::none: return io_type::none;
    case io_type::in: return io_type::out;
    case io_type::out: return io_type::in;
    case io_type::bidir: return io_type::bidir;
    }
    return io;
}

constexpr auto to_cstring(io_type io) noexcept -> const char*
{
    switch (io) {
    case io_type::none: return "none";
    case io_type::in: return "in";
    case io_type::out: return "out";
    case io_type::bidir: return "in|out";
    }
    return "unknown";
}

constexpr auto to_io_type(const std::string_view& s) -> std::optional<io_type>
{
    for (const auto dir: std::initializer_list<io_type>{
        io_type::none, io_type::in, io_type::out, io_type::bidir,
    }) {
        if (s == to_cstring(dir)) {
            return dir;
        }
    }
    return {};
}

auto operator<<(std::ostream& os, io_type value) -> std::ostream&;

}

#endif /* io_type_hpp */
