#ifndef channel_hpp
#define channel_hpp

#include <array>
#include <ostream>
#include <type_traits> // for std::is_nothrow_move_*
#include <variant>

#include "flow/descriptor_id.hpp"
#include "flow/io_type.hpp"

namespace flow {

/// @brief File channel.
/// @note This is intended to be a strong place-holder type.
struct file_channel {
};

/// @brief POSIX pipe.
/// @note This class is movable but not copyable.
struct pipe_channel {
    /// @throws std::runtime_error if the underlying OS call fails.
    pipe_channel();

    pipe_channel(pipe_channel&& other) noexcept;

    ~pipe_channel() noexcept;

    pipe_channel& operator=(pipe_channel&& other) noexcept;

    // This class is not meant to be copied!
    pipe_channel(const pipe_channel& other) = delete;
    pipe_channel& operator=(const pipe_channel& other) = delete;

    bool close(io_type direction, std::ostream& os) noexcept;
    bool dup(io_type direction, descriptor_id newfd, std::ostream& os) noexcept;

    friend std::ostream& operator<<(std::ostream& os, const pipe_channel& p);

private:
    std::array<int, 2> descriptors{-1, -1};
};

static_assert(std::is_nothrow_move_constructible_v<pipe_channel>);
static_assert(std::is_nothrow_move_assignable_v<pipe_channel>);

using channel = std::variant<pipe_channel, file_channel>;

std::ostream& operator<<(std::ostream& os, const file_channel& value);
std::ostream& operator<<(std::ostream& os, const pipe_channel& value);

}

#endif /* channel_hpp */
