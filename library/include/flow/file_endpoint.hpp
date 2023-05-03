#ifndef file_endpoint_hpp
#define file_endpoint_hpp

#include <filesystem>
#include <ostream>

namespace flow {

struct file_endpoint {
    std::filesystem::path path;
};

inline auto operator==(const file_endpoint& lhs, const file_endpoint& rhs) -> bool
{
    return lhs.path == rhs.path;
}

std::ostream& operator<<(std::ostream& os, const file_endpoint& value);

}

#endif /* file_endpoint_hpp */
