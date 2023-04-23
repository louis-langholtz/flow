#include <cerrno> // for errno
#include <cstring> // for std::streror
#include <iostream>
#include <sstream> // for std::ostringstream
#include <stdexcept> // for std::runtime_error
#include <utility> // for std::exchange

#include <unistd.h> // for pipe, close

#include "flow/pipe_channel.hpp"

namespace flow {

pipe_channel::pipe_channel()
{
    if (::pipe(value.data()) == -1) {
        throw std::runtime_error{std::strerror(errno)};
    }
}

pipe_channel::pipe_channel(pipe_channel&& other) noexcept: value{std::exchange(other.value, {-1, -1})}
{
    // Intentionally empty.
}

pipe_channel::~pipe_channel() noexcept
{
    for (auto&& d: value) {
        if (d != -1) {
            ::close(d);
            std::cerr << "pipe closed\n";
        }
    }
}

pipe_channel& pipe_channel::operator=(pipe_channel&& other) noexcept
{
    value = std::exchange(other.value, {-1, -1});
    return *this;
}

bool pipe_channel::close(io_type direction, std::ostream& os) noexcept
{
    auto& d = value[int(direction)];
    if (d != -1) {
        if (::close(d) == -1) {
            os << "close(" << direction << "," << d << ") failed: ";
            os << std::strerror(errno);
            return false;
        }
        d = -1;
    }
    return true;
}

bool pipe_channel::dup(io_type direction, descriptor_id newfd,
                       std::ostream& os) noexcept
{
    const auto new_d = int(newfd);
    auto& d = value[int(direction)];
    if (dup2(d, new_d) == -1) {
        os << "dup2(" << direction << ":" << d << "," << new_d << ") failed: ";
        os << std::strerror(errno);
        return false;
    }
    d = new_d;
    return true;
}

std::ostream& operator<<(std::ostream& os, const pipe_channel& p)
{
    os << "pipe_channel{" << p.value[0] << "," << p.value[1] << "}";
    return os;
}

}
