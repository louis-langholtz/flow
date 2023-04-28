#ifndef wait_status_hpp
#define wait_status_hpp

#include <ostream>
#include <variant>

namespace flow {

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

using wait_status = std::variant<
    std::monostate,
    wait_exit_status,
    wait_signaled_status,
    wait_stopped_status,
    wait_continued_status
>;

}

#endif /* wait_status_hpp */
