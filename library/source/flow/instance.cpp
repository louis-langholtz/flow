#include <unistd.h> // for getpid

#include "flow/instance.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const instance& value)
{
    os << "instance{";
    os << ".id=" << value.id;
    os << ",.children={";
    for (auto&& entry: value.children) {
        if (&entry != &(*value.children.begin())) {
            os << ",";
        }
        os << "{";
        os << ".first=" << entry.first;
        os << ",.second=" << entry.second;
        os << "}";
    }
    os << "}";
    os << ",.channels={";
    for (auto&& elem: value.channels) {
        if (&elem != &(*value.channels.begin())) {
            os << ",";
        }
        os << elem;
    }
    os << "}";
    os << "}";
    return os;
}

auto temporary_fstream() -> fstream
{
    // "w+xb"
    constexpr auto mode =
        fstream::in|fstream::out|fstream::trunc|fstream::binary|fstream::noreplace|fstream::tmpfile;

    fstream stream;
    stream.open(std::filesystem::temp_directory_path(), mode);
    return stream;
}

}
