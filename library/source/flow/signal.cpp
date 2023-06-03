#include <csignal>

#include "flow/signal.hpp"

namespace flow {

auto operator<<(std::ostream& os, signal s) -> std::ostream&
{
    switch (int(s)) {
    case SIGINT:
        os << "sigint";
        break;
    case SIGTERM:
        os << "sigterm";
        break;
    case SIGKILL:
        os << "sigkill";
        break;
    case SIGCONT:
        os << "sigcont";
        break;
    case SIGCHLD:
        os << "sigchild";
        break;
    case SIGWINCH:
        os << "sigwinch";
        break;
    default:
        os << "signal-#" << std::to_string(int(s));
        break;
    }
    return os;
}

}

namespace flow::signals {

auto interrupt() noexcept -> signal
{
    return signal{SIGINT};
}

auto terminate() noexcept -> signal
{
    return signal{SIGTERM};
}

auto kill() noexcept -> signal
{
    return signal{SIGKILL};
}

auto cont() noexcept -> signal
{
    return signal{SIGCONT};
}

auto child() noexcept -> signal
{
    return signal{SIGCHLD};
}

auto winch() noexcept -> signal
{
    return signal{SIGWINCH};
}

}
