#ifndef wait_status_hpp
#define wait_status_hpp

#include <concepts> // for std::regular.
#include <compare> // for std::strong_ordering
#include <ostream>

#include "flow/variant.hpp" // for <variant>, flow::variant, + ostream support

namespace flow {

struct wait_unknown_status {
    constexpr auto operator<=>(const wait_unknown_status&) const noexcept = default;
};

auto operator<<(std::ostream& os, const wait_unknown_status& value)
    -> std::ostream&;

struct wait_exit_status {
    int value{};
    constexpr auto operator<=>(const wait_exit_status& other) const = default;
};

auto operator<<(std::ostream& os, const wait_exit_status& value)
    -> std::ostream&;

struct wait_signaled_status {
    int signal{};
    bool core_dumped{};
    constexpr auto operator<=>(const wait_signaled_status& other) const = default;
};

auto operator<<(std::ostream& os, const wait_signaled_status& value)
    -> std::ostream&;

struct wait_stopped_status {
    int stop_signal{};
    constexpr auto operator<=>(const wait_stopped_status& other) const = default;
};

auto operator<<(std::ostream& os, const wait_stopped_status& value)
    -> std::ostream&;

struct wait_continued_status {
    constexpr auto operator<=>(const wait_continued_status& other) const = default;
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

static_assert(std::regular<wait_status>);

}

#endif /* wait_status_hpp */
