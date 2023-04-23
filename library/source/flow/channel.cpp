#include <cerrno> // for errno
#include <cstring> // for std::streror
#include <iostream>
#include <sstream> // for std::ostringstream
#include <stdexcept> // for std::runtime_error
#include <utility> // for std::exchange

#include <unistd.h> // for pipe, close

#include "flow/channel.hpp"

namespace flow {

pipe_channel::pipe_channel()
{
    if (::pipe(descriptors.data()) == -1) {
        throw std::runtime_error{std::strerror(errno)}; // NOLINT(concurrency-mt-unsafe)
    }
}

pipe_channel::pipe_channel(pipe_channel&& other) noexcept: descriptors{std::exchange(other.descriptors, {-1, -1})}
{
    // Intentionally empty.
}

pipe_channel::~pipe_channel() noexcept
{
    for (auto&& d: descriptors) {
        if (d != -1) {
            ::close(d);
            std::cerr << "pipe closed\n";
        }
    }
}

pipe_channel& pipe_channel::operator=(pipe_channel&& other) noexcept
{
    descriptors = std::exchange(other.descriptors, {-1, -1});
    return *this;
}

bool pipe_channel::close(io_type direction, std::ostream& os) noexcept
{
    auto& d = descriptors[int(direction)];
    if (d != -1) {
        if (::close(d) == -1) {
            os << "close(" << direction << "," << d << ") failed: ";
            os << std::strerror(errno); // NOLINT(concurrency-mt-unsafe)
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
    auto& d = descriptors[int(direction)];
    if (dup2(d, new_d) == -1) {
        os << "dup2(" << direction << ":" << d << "," << new_d << ") failed: ";
        os << std::strerror(errno); // NOLINT(concurrency-mt-unsafe)
        return false;
    }
    d = new_d;
    return true;
}

std::ostream& operator<<(std::ostream& os, const file_channel&)
{
    os << "file_channel{}";
    return os;
}

std::ostream& operator<<(std::ostream& os, const pipe_channel& value)
{
    os << "pipe_channel{";
    os << value.descriptors[0];
    os << ",";
    os << value.descriptors[1];
    os << "}";
    return os;
}

std::ostream& operator<<(std::ostream& os, const channel& value)
{
    if (const auto c = std::get_if<pipe_channel>(&value)) {
        os << *c;
    }
    else if (const auto c = std::get_if<file_channel>(&value)) {
        os << *c;
    }
    return os;
}

}
