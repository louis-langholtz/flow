#ifndef file_endpoint_hpp
#define file_endpoint_hpp

#include <filesystem>
#include <ostream>

namespace flow {

struct file_endpoint {
    static const file_endpoint dev_null;

    std::filesystem::path path;
    auto operator==(const file_endpoint&) const -> bool = default;
};

std::ostream& operator<<(std::ostream& os, const file_endpoint& value);

}

#endif /* file_endpoint_hpp */
