#ifndef file_port_hpp
#define file_port_hpp

#include <filesystem>
#include <ostream>

namespace flow {

struct file_endpoint {
    std::filesystem::path path;
};

constexpr auto operator==(const file_endpoint& lhs, const file_endpoint& rhs) -> bool
{
    return lhs.path == rhs.path;
}

std::ostream& operator<<(std::ostream& os, const file_endpoint& value);

}

#endif /* file_port_hpp */
