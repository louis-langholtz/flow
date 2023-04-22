#include <cerrno> // for errno
#include <cstring> // for std::streror
#include <iostream>
#include <sstream> // for std::ostringstream
#include <stdexcept> // for std::runtime_error
#include <utility> // for std::exchange

#include <unistd.h> // for pipe, close

#include "pipe.hpp"

namespace flow {

pipe::pipe()
{
    if (::pipe(value.data()) == -1) {
        throw std::runtime_error{std::strerror(errno)};
    }
}

pipe::pipe(pipe&& other) noexcept: value{std::exchange(other.value, {-1, -1})}
{
    // Intentionally empty.
}

pipe::~pipe() noexcept
{
    for (auto&& d: value) {
        if (d != -1) {
            ::close(d);
            std::cerr << "pipe closed\n";
        }
    }
}

pipe& pipe::operator=(pipe&& other) noexcept
{
    value = std::exchange(other.value, {-1, -1});
    return *this;
}

void pipe::close(io_type direction)
{
    auto& d = value[int(direction)];
    if (d != -1) {
        if (::close(d) == -1) {
            std::ostringstream os;
            os << "close(" << direction << ", " << d << ") failed: ";
            os << std::strerror(errno);
            throw std::runtime_error{os.str()};
        }
        d = -1;
    }
}

void pipe::dup(io_type direction, descriptor_id newfd)
{
    const auto new_d = int(newfd);
    auto& d = value[int(direction)];
    if (dup2(d, new_d) == -1) {
        std::ostringstream os;
        os << "dup2(" << direction << ":" << d << "," << new_d << ") failed: ";
        os << std::strerror(errno);
        throw std::runtime_error{os.str()};
    }
    d = new_d;
}

std::ostream& operator<<(std::ostream& os, const pipe& p)
{
    os << "{";
    os << p.value[0];
    os << ",";
    os << p.value[1];
    os << "}";
    return os;
}

}
