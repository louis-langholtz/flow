#ifndef channel_hpp
#define channel_hpp

#include <array>
#include <functional> // for std::reference_wrapper
#include <ostream>
#include <span>
#include <type_traits> // for std::is_nothrow_move_*

#include "flow/descriptor_id.hpp"
#include "flow/io_type.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

/// @brief File channel.
/// @note This is intended to be a strong place-holder/tag type.
/// @note Instances of this type are made for <code>file_connection</code> instances.
/// @see file_connection.
struct file_channel {
    io_type io{};
};

/// @brief POSIX pipe.
/// @note This class is movable but not copyable.
/// @note Instances of this type are made for <code>pipe_connection</code> instances.
/// @see pipe_connection.
struct pipe_channel {

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

    /// @note This function is NOT thread safe in error cases.
    auto close(io side, std::ostream& errs) noexcept -> bool;

    /// @note This function is NOT thread safe in error cases.
    auto dup(io side, descriptor_id newfd,
             std::ostream& errs) noexcept -> bool;

    auto read(const std::span<char>& buffer, std::ostream& errs) const
        -> std::size_t;

    auto write(const std::span<const char>& buffer, std::ostream& errs) const
        -> bool;

    friend auto operator<<(std::ostream& os, const pipe_channel& value)
        -> std::ostream&;

private:
    /// @brief First element is read side of pipe, second is write side.
    std::array<int, 2> descriptors{-1, -1};
};

static_assert(std::is_nothrow_move_constructible_v<pipe_channel>);
static_assert(std::is_nothrow_move_assignable_v<pipe_channel>);

struct reference_channel;

using channel = variant<file_channel, pipe_channel, reference_channel>;

struct reference_channel {
    /// @brief Non-owning pointer to referenced channel.
    channel *other{};
};

auto operator<<(std::ostream& os, const file_channel& value) -> std::ostream&;

auto operator<<(std::ostream& os, pipe_channel::io value)
    -> std::ostream&;

auto operator<<(std::ostream& os, const pipe_channel& value) -> std::ostream&;

auto operator<<(std::ostream& os, const reference_channel& value)
    -> std::ostream&;

}

#endif /* channel_hpp */
