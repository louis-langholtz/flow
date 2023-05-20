#ifndef pipe_channel_hpp
#define pipe_channel_hpp

#include <array>
#include <ostream>
#include <span>
#include <sstream> // for std::ostringstream
#include <type_traits> // for std::is_nothrow_move_*

#include "flow/reference_descriptor.hpp"

namespace flow {

namespace detail {

constexpr auto default_pipe_read_buffer_size = 4096u;

}

/// @brief POSIX pipe.
/// @note This class is movable but not copyable.
/// @note Instances of this type are made for
/// <code>unidirectional_connection</code> instances.
/// @see unidirectional_connection.
struct pipe_channel
{
    enum class io: unsigned {read = 0u, write = 1u};

    /// @note This function is NOT thread safe in error cases.
    /// @throws std::runtime_error if the underlying OS call fails.
    pipe_channel();

    pipe_channel(pipe_channel&& other) noexcept;

    ~pipe_channel() noexcept;

    auto operator=(pipe_channel&& other) noexcept -> pipe_channel&;

    // This class is not meant to be copied!
    pipe_channel(const pipe_channel& other) = delete;
    auto operator=(const pipe_channel& other) -> pipe_channel& = delete;

    auto close() noexcept -> bool;

    /// @note This function is NOT thread safe in error cases.
    auto close(io side, std::ostream& diags) noexcept -> bool;

    /// @note This function is NOT thread safe in error cases.
    auto dup(io side, reference_descriptor newfd,
             std::ostream& diags) noexcept -> bool;

    auto read(const std::span<char>& buffer, std::ostream& diags) const
    -> std::size_t;

    auto write(const std::span<const char>& buffer, std::ostream& diags) const
    -> bool;

    friend auto operator<<(std::ostream& os, const pipe_channel& value)
    -> std::ostream&;

private:
    /// @brief First element is read side of pipe, second is write side.
    std::array<int, 2u> descriptors{-1, -1};
};

static_assert(!std::is_copy_constructible_v<pipe_channel>);
static_assert(!std::is_copy_assignable_v<pipe_channel>);
static_assert(std::is_nothrow_move_constructible_v<pipe_channel>);
static_assert(std::is_nothrow_move_assignable_v<pipe_channel>);

auto operator<<(std::ostream& os, pipe_channel::io value)
-> std::ostream&;

auto operator<<(std::ostream& os, const pipe_channel& value) -> std::ostream&;

template <class OutputIt,
          std::size_t buffer_size = detail::default_pipe_read_buffer_size>
auto read(const pipe_channel& pipe, OutputIt out_it)
    -> OutputIt
{
    std::array<char, buffer_size> buffer{};
    for (;;) {
        std::ostringstream os;
        using ret_type = decltype(pipe.read(buffer, os));
        const auto nread = pipe.read(buffer, os);
        if (nread == static_cast<ret_type>(-1)) {
            throw std::runtime_error{os.str()};
        }
        if (nread == ret_type{}) {
            break;
        }
        out_it = std::copy(data(buffer), data(buffer) + nread, out_it);
    }
    return out_it;
}

auto write(pipe_channel& pipe, const std::span<const char>& data) -> void;

}

#endif /* pipe_channel_hpp */
