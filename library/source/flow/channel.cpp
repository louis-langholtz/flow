#include <cerrno> // for errno
#include <cstring> // for std::streror
#include <iostream>
#include <sstream> // for std::ostringstream
#include <stdexcept> // for std::runtime_error
#include <utility> // for std::exchange

#include <unistd.h> // for pipe, close

#include "system_error_code.hpp"

#include "flow/channel.hpp"

namespace flow {

pipe_channel::pipe_channel()
{
    if (::pipe(descriptors.data()) == -1) {
        throw std::runtime_error{to_string(system_error_code(errno))};
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

bool pipe_channel::close(io side, std::ostream& errs) noexcept
{
    auto& d = descriptors[int(side)];
    if (::close(d) == -1) {
        errs << "close(" << side << "," << d << ") failed: ";
        errs << system_error_code(errno) << "\n";
        return false;
    }
    d = -1;
    return true;
}

bool pipe_channel::dup(io side, descriptor_id newfd,
                       std::ostream& errs) noexcept
{
    const auto new_d = int(newfd);
    auto& d = descriptors[int(side)];
    if (dup2(d, new_d) == -1) {
        errs << "dup2(" << side << ":" << d << "," << new_d << ") failed: ";
        errs << system_error_code(errno) << "\n";
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
        errs << system_error_code(errno) << "\n";
        return static_cast<std::size_t>(-1);
    }
    return static_cast<std::size_t>(nread);
}

bool pipe_channel::write(const std::span<const char>& buffer,
                         std::ostream& errs) const
{
    if (::write(descriptors[1], buffer.data(), buffer.size()) == -1) {
        errs << "write(fd=" << descriptors[1] << ",siz=" << buffer.size() << ") failed: ";
        errs << system_error_code(errno) << "\n";
        return false;
    }
    return true;
}

auto operator<<(std::ostream& os, const file_channel&) -> std::ostream&
{
    os << "file_channel{}";
    return os;
}

auto operator<<(std::ostream& os, pipe_channel::io value)
    -> std::ostream&
{
    os << ((value == pipe_channel::io::read) ? "read": "write");
    return os;
}

auto operator<<(std::ostream& os, const pipe_channel& value) -> std::ostream&
{
    os << "pipe_channel{";
    os << value.descriptors[0];
    os << ",";
    os << value.descriptors[1];
    os << "}";
    return os;
}

auto operator<<(std::ostream& os, const reference_channel& value)
    -> std::ostream&
{
    os << "reference_channel{";
    if (value.other) {
        os << *value.other;
    }
    os << "}";
    return os;
}

}
