#ifndef wait_option_hpp
#define wait_option_hpp

namespace flow {

enum class wait_option: int;

constexpr auto operator|(const wait_option& lhs,
                         const wait_option& rhs) noexcept
    -> wait_option
{
    return wait_option(int(lhs) | int(rhs));
}

constexpr auto operator&(const wait_option& lhs,
                         const wait_option& rhs) noexcept
    -> wait_option
{
    return wait_option(int(lhs) & int(rhs));
}

constexpr auto operator~(const wait_option& val) noexcept
    -> wait_option
{
    return wait_option(~int(val));
}

namespace detail {
auto get_nohang_wait_option() noexcept -> wait_option;
auto get_untraced_wait_option() noexcept -> wait_option;
}

namespace wait_options {
const wait_option nohang = detail::get_nohang_wait_option();
const wait_option untraced = detail::get_untraced_wait_option();
}

}

#endif /* wait_option_hpp */
