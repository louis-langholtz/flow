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
        }
    }
}

pipe_channel& pipe_channel::operator=(pipe_channel&& other) noexcept
{
    descriptors = std::exchange(other.descriptors, {-1, -1});
    return *this;
}

bool pipe_channel::close(io_type direction, std::ostream& errs) noexcept
{
    auto& d = descriptors[int(direction)];
    if (::close(d) == -1) {
        errs << "close(" << direction << "," << d << ") failed: ";
        errs << std::strerror(errno) << "\n"; // NOLINT(concurrency-mt-unsafe)
        return false;
    }
    d = -1;
    return true;
}

bool pipe_channel::dup(io_type direction, descriptor_id newfd,
                       std::ostream& errs) noexcept
{
    const auto new_d = int(newfd);
    auto& d = descriptors[int(direction)];
    if (dup2(d, new_d) == -1) {
        errs << "dup2(" << direction << ":" << d << "," << new_d << ") failed: ";
        errs << std::strerror(errno) << "\n"; // NOLINT(concurrency-mt-unsafe)
        return false;
    }
    d = new_d;
    return true;
}

std::size_t pipe_channel::read(const std::span<char>& buffer,
                               std::ostream& errs) const
{
    const auto nread = ::read(descriptors[0], buffer.data(), buffer.size());
    if (nread == -1) {
        errs << "read() failed: ";
        errs << std::strerror(errno) << "\n"; // NOLINT(concurrency-mt-unsafe)
        return static_cast<std::size_t>(-1);
    }
    return static_cast<std::size_t>(nread);
}

bool pipe_channel::write(const std::span<const char>& buffer,
                         std::ostream& errs) const
{
    if (::write(descriptors[1], buffer.data(), buffer.size()) == -1) {
        errs << "write(fd=" << descriptors[1] << ",siz=" << buffer.size() << ") failed: ";
        errs << std::strerror(errno) << "\n"; // NOLINT(concurrency-mt-unsafe)
        return false;
    }
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
