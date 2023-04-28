#include "wait_status.hpp"

namespace flow {

auto operator<<(std::ostream& os, const wait_exit_status& value) -> std::ostream&
{
    os << "exit-status=" << value.value;
    return os;
}

auto operator<<(std::ostream& os, const wait_signaled_status& value) -> std::ostream&
{
    os << "signal=" << value.signal;
    os << ", core-dumped=" << std::boolalpha << value.core_dumped;
    return os;
}

auto operator<<(std::ostream& os, const wait_stopped_status& value) -> std::ostream&
{
    os << "stop-signal=" << value.stop_signal;
    return os;
}

auto operator<<(std::ostream& os, const wait_continued_status&) -> std::ostream&
{
    os << "continued";
    return os;
}

}
