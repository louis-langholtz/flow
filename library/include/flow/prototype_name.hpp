#ifndef process_name_hpp
#define process_name_hpp

#include <ostream>
#include <string>

namespace flow {

struct prototype_name {
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

}

#endif /* process_name_hpp */
