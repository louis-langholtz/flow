#ifndef process_name_hpp
#define process_name_hpp

#include <ostream>
#include <string>

namespace flow {

struct process_name {
    std::string value;
};

constexpr auto operator==(const process_name& lhs, const process_name& rhs) noexcept
{
    return lhs.value == rhs.value;
}

constexpr auto operator<(const process_name& lhs, const process_name& rhs) noexcept
{
    return lhs.value < rhs.value;
}

std::ostream& operator<<(std::ostream& os, const process_name& name);

}

#endif /* process_name_hpp */
