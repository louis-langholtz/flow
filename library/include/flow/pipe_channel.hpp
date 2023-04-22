#ifndef flow_pipe_hpp
#define flow_pipe_hpp

#include <array>
#include <type_traits> // for std::is_nothrow_move_*

#include "flow/descriptor_id.hpp"
#include "flow/io_type.hpp"

namespace flow {

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

    void close(io_type direction);
    void dup(io_type direction, descriptor_id newfd);

    friend std::ostream& operator<<(std::ostream& os, const pipe_channel& p);

private:
    std::array<int, 2> value;
};

static_assert(std::is_nothrow_move_constructible_v<pipe_channel>);
static_assert(std::is_nothrow_move_assignable_v<pipe_channel>);

std::ostream& operator<<(std::ostream& os, const pipe_channel& p);

}

#endif /* flow_pipe_hpp */
