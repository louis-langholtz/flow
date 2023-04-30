#ifndef wait_status_hpp
#define wait_status_hpp

#include <compare> // for std::strong_ordering
#include <ostream>

#include "flow/variant.hpp" // for <variant>, flow::variant, plus ostream support

namespace flow {

struct wait_unknown_status {
};

constexpr std::strong_ordering operator<=>(wait_unknown_status, wait_unknown_status) noexcept
{
    return std::strong_ordering::equal;
}

auto operator<<(std::ostream& os, const wait_unknown_status& value)
    -> std::ostream&;

struct wait_exit_status {
    int value{};
};

auto operator<<(std::ostream& os, const wait_exit_status& value)
    -> std::ostream&;

struct wait_signaled_status {
    int signal{};
    bool core_dumped{};
};

auto operator<<(std::ostream& os, const wait_signaled_status& value)
    -> std::ostream&;

struct wait_stopped_status {
    int stop_signal{};
};

auto operator<<(std::ostream& os, const wait_stopped_status& value)
    -> std::ostream&;

struct wait_continued_status {
};

auto operator<<(std::ostream& os, const wait_continued_status&)
    -> std::ostream&;

using wait_status = variant<
    wait_unknown_status,
    wait_exit_status,
    wait_signaled_status,
    wait_stopped_status,
    wait_continued_status
>;

}

#endif /* wait_status_hpp */
