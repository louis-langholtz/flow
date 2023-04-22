#ifndef flow_pipe_hpp
#define flow_pipe_hpp

#include <array>
#include <type_traits> // for std::is_nothrow_move_*

#include "flow/descriptor_id.hpp"
#include "flow/io_type.hpp"

namespace flow {

/// @brief POSIX pipe.
/// @note This class is movable but not copyable.
struct pipe {
    /// @throws std::runtime_error if the underlying OS call fails.
    pipe();

    pipe(pipe&& other) noexcept;

    ~pipe() noexcept;

    pipe& operator=(pipe&& other) noexcept;

    // This class is not meant to be copied!
    pipe(const pipe& other) = delete;
    pipe& operator=(const pipe& other) = delete;

    void close(io_type direction);
    void dup(io_type direction, descriptor_id newfd);

    friend std::ostream& operator<<(std::ostream& os, const pipe& p);

private:
    std::array<int, 2> value;
};

static_assert(std::is_nothrow_move_constructible_v<pipe>);
static_assert(std::is_nothrow_move_assignable_v<pipe>);

std::ostream& operator<<(std::ostream& os, const pipe& p);

}

#endif /* flow_pipe_hpp */
