#ifndef file_port_hpp
#define file_port_hpp

#include <filesystem>
#include <ostream>

namespace flow {

struct file_port {
    std::filesystem::path path;
};

std::ostream& operator<<(std::ostream& os, const file_port& value);

}

#endif /* file_port_hpp */
