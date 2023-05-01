#ifndef process_name_hpp
#define process_name_hpp

#include <ostream>
#include <string>

namespace flow {

struct prototype_name {
    static constexpr auto separator = ".";
    std::string value;
};

constexpr auto operator==(const prototype_name& lhs,
                          const prototype_name& rhs) noexcept
{
    return lhs.value == rhs.value;
}

constexpr auto operator<(const prototype_name& lhs,
                         const prototype_name& rhs) noexcept
{
    return lhs.value < rhs.value;
}

auto operator<<(std::ostream& os, const prototype_name& name) -> std::ostream&;

auto operator+(const prototype_name& lhs, const prototype_name& rhs)
    -> prototype_name;

}

#endif /* process_name_hpp */
