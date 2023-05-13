#ifndef system_name_hpp
#define system_name_hpp

#include <ostream>
#include <string>
#include <vector>

namespace flow {

struct system_name {
    static constexpr auto separator = ".";
    std::string value;
};

inline auto operator==(const system_name& lhs,
                       const system_name& rhs) noexcept
{
    return lhs.value == rhs.value;
}

inline auto operator<(const system_name& lhs,
                      const system_name& rhs) noexcept
{
    return lhs.value < rhs.value;
}

auto operator<<(std::ostream& os, const system_name& name) -> std::ostream&;

auto operator+(const system_name& lhs, const system_name& rhs)
    -> system_name;

auto operator<<(std::ostream& os,
                const std::vector<system_name>& names) -> std::ostream&;

}

#endif /* system_name_hpp */
