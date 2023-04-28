#ifndef channel_hpp
#define channel_hpp

#include <array>
#include <ostream>
#include <span>
#include <type_traits> // for std::is_nothrow_move_*

#include "flow/descriptor_id.hpp"
#include "flow/io_type.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, plus ostream support

namespace flow {

/// @brief File channel.
/// @note This is intended to be a strong place-holder/tag type.
/// @note Instances of this type are made for <code>file_connection</code> instances.
/// @see file_connection.
struct file_channel {
};

/// @brief POSIX pipe.
/// @note This class is movable but not copyable.
/// @note Instances of this type are made for <code>pipe_connection</code> instances.
/// @see pipe_connection.
struct pipe_channel {
    /// @note This function is NOT thread safe in error cases.
    /// @throws std::runtime_error if the underlying OS call fails.
    pipe_channel();

    pipe_channel(pipe_channel&& other) noexcept;

    ~pipe_channel() noexcept;

    pipe_channel& operator=(pipe_channel&& other) noexcept;

    // This class is not meant to be copied!
    pipe_channel(const pipe_channel& other) = delete;
    pipe_channel& operator=(const pipe_channel& other) = delete;

    /// @note This function is NOT thread safe in error cases.
    bool close(io_type direction, std::ostream& errs) noexcept;

    /// @note This function is NOT thread safe in error cases.
    bool dup(io_type direction, descriptor_id newfd,
             std::ostream& errs) noexcept;

    std::size_t read(const std::span<char>& buffer,
                     std::ostream& errs) const;

    bool write(const std::span<const char>& buffer,
               std::ostream& errs) const;

    friend std::ostream& operator<<(std::ostream& os,
                                    const pipe_channel& value);

private:
    std::array<int, 2> descriptors{-1, -1};
};

static_assert(std::is_nothrow_move_constructible_v<pipe_channel>);
static_assert(std::is_nothrow_move_assignable_v<pipe_channel>);

using channel = variant<pipe_channel, file_channel>;

std::ostream& operator<<(std::ostream& os, const file_channel& value);
std::ostream& operator<<(std::ostream& os, const pipe_channel& value);

}

#endif /* channel_hpp */
