#ifndef signal_hpp
#define signal_hpp

#include <ostream>

namespace flow {

enum class signal: int;

auto operator<<(std::ostream& os, signal s) -> std::ostream&;

namespace signals {
auto interrupt() noexcept -> signal;
auto terminate() noexcept -> signal;
auto kill() noexcept -> signal;
auto cont() noexcept -> signal;
auto child() noexcept -> signal;
auto winch() noexcept -> signal;
}

}

#endif /* signal_hpp */
