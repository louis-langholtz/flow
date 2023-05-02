#ifndef process_name_hpp
#define process_name_hpp

#include <ostream>
#include <string>

namespace flow {

struct system_name {
    static constexpr auto separator = ".";
    std::string value;
};

constexpr auto operator==(const system_name& lhs,
                          const system_name& rhs) noexcept
{
    return lhs.value == rhs.value;
}

constexpr auto operator<(const system_name& lhs,
                         const system_name& rhs) noexcept
{
    return lhs.value < rhs.value;
}

auto operator<<(std::ostream& os, const system_name& name) -> std::ostream&;

auto operator+(const system_name& lhs, const system_name& rhs)
    -> system_name;

}

#endif /* process_name_hpp */
