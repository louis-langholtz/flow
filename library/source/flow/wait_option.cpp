#include <sys/wait.h> // for WNOHANG, WUNTRACED

#include "flow/wait_option.hpp"

namespace flow::wait_options {

auto nohang() noexcept -> wait_option
{
    return wait_option(WNOHANG);
}

auto untraced() noexcept -> wait_option
{
    return wait_option(WUNTRACED);
}

}
