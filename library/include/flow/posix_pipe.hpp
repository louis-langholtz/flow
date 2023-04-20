#ifndef posix_pipe_hpp
#define posix_pipe_hpp

#include <array>
#include <type_traits> // for std::is_nothrow_move_*

#include "flow/descriptor_id.hpp"
#include "flow/io_type.hpp"

namespace flow {

/// @brief POSIX pipe.
/// @note This class is movable but not copyable.
struct posix_pipe {
    /// @throws std::runtime_error if the underlying OS call fails.
    posix_pipe();

    posix_pipe(posix_pipe&& other) noexcept;

    ~posix_pipe() noexcept;

    posix_pipe& operator=(posix_pipe&& other) noexcept;

    // This class is not meant to be copied!
    posix_pipe(const posix_pipe& other) = delete;
    posix_pipe& operator=(const posix_pipe& other) = delete;

    void close(io_type direction);
    void dup(io_type direction, descriptor_id newfd);

    friend std::ostream& operator<<(std::ostream& os,
                                    const posix_pipe& p);

private:
    std::array<int, 2> value;
};

static_assert(std::is_nothrow_move_constructible_v<posix_pipe>);
static_assert(std::is_nothrow_move_assignable_v<posix_pipe>);

std::ostream& operator<<(std::ostream& os, const posix_pipe& p);

}

#endif /* posix_pipe_hpp */
