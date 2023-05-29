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

namespace wait_options {
auto nohang() noexcept -> wait_option;
auto untraced() noexcept -> wait_option;
}

}

#endif /* wait_option_hpp */
