#ifndef process_name_hpp
#define process_name_hpp

#include <ostream>
#include <string>

namespace flow {

struct prototype_name {
    static constexpr auto separator = ".";
    std::string value;
};

constexpr auto operator==(const prototype_name& lhs, const prototype_name& rhs) noexcept
{
    return lhs.value == rhs.value;
}

constexpr auto operator<(const prototype_name& lhs, const prototype_name& rhs) noexcept
{
    return lhs.value < rhs.value;
}

std::ostream& operator<<(std::ostream& os, const prototype_name& name);

prototype_name operator+(const prototype_name& lhs, const prototype_name& rhs);

}

#endif /* process_name_hpp */
