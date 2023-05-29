#include <sys/wait.h> // for WNOHANG, WUNTRACED

#include "flow/wait_option.hpp"

namespace flow::detail {

auto get_nohang_wait_option() noexcept -> wait_option
{
    return wait_option(WNOHANG);
}

auto get_untraced_wait_option() noexcept -> wait_option
{
    return wait_option(WUNTRACED);
}

}
