#ifndef restricted_string_h
#define restricted_string_h

#include <string>
#include <type_traits>

namespace flow::detail {

template <class T, class Checker>
struct checked_value
{
    using value_type = T;
    using checker_type = Checker;

    constexpr checked_value(): data{checker_type{}()} {}

    checked_value(const checked_value& other) = default;

    checked_value(checked_value&& other) // NOLINT(bugprone-exception-escape)
    noexcept(std::is_nothrow_move_constructible_v<value_type>) = default;

    template <class U, class V = std::enable_if_t<
        !std::is_same_v<std::decay_t<U>, checked_value> &&
        std::is_constructible_v<value_type, U>
    >>
    checked_value(U&& u)
        : data{checker_type{}(std::forward<U>(u))}
    {
        // Intentionally empty.
    }

    template<class InputIt, class U = std::enable_if_t<
        std::is_constructible_v<value_type, InputIt, InputIt>
    >>
    checked_value(InputIt first, InputIt last)
        : data{checker_type{}(first, last)}
    {
        // Intentionally empty.
    }

    auto operator=(const checked_value& other) -> checked_value& = default;

    auto operator=(checked_value&& other) // NOLINT(bugprone-exception-escape)
        noexcept(std::is_nothrow_move_assignable_v<value_type>)
        -> checked_value& = default;

    constexpr explicit operator value_type() const
    {
        return data;
    }

    [[nodiscard]] auto get() const & noexcept -> const value_type&
    {
        return data;
    }

private:
    value_type data;
};

template <class LhsV, class LhsC, class RhsV, class RhsC>
inline auto operator==(const checked_value<LhsV, LhsC>& lhs,
                       const checked_value<RhsV, RhsC>& rhs)
    -> decltype(LhsV(lhs) == RhsV(rhs))
{
    return LhsV(lhs) == RhsV(rhs);
}

template <class LhsV, class LhsC, class RhsV, class RhsC>
inline auto operator<(const checked_value<LhsV, LhsC>& lhs,
                      const checked_value<RhsV, RhsC>& rhs)
    -> decltype(LhsV(lhs) < RhsV(rhs))
{
    return LhsV(lhs) < RhsV(rhs);
}

template <class V, class C>
inline auto operator==(const checked_value<V, C>& checked,
                       const V& value)
    -> decltype(V(checked) == value)
{
    return V(checked) == value;
}

template <class V, class C>
inline auto operator==(const V& value,
                       const checked_value<V, C>& checked)
    -> decltype(V(checked) == value)
{
    return V(checked) == value;
}

template <class V, class C>
inline auto operator<(const checked_value<V, C>& checked,
                      const V& value)
    -> decltype(V(checked) < value)
{
    return V(checked) < value;
}

template <class V, class C>
inline auto operator<(const V& value,
                      const checked_value<V, C>& checked)
    -> decltype(V(checked) == value)
{
    return V(checked) == value;
}

template <class V, class C>
inline auto operator<=(const checked_value<V, C>& checked,
                       const V& value)
    -> decltype(V(checked) <= value)
{
    return V(checked) <= value;
}

template <class V, class C>
inline auto operator<=(const V& value,
                       const checked_value<V, C>& checked)
    -> decltype(V(checked) <= value)
{
    return V(checked) == value;
}

template <class V, class C>
inline auto operator>(const checked_value<V, C>& checked,
                      const V& value)
    -> decltype(V(checked) > value)
{
    return V(checked) > value;
}

template <class V, class C>
inline auto operator>(const V& value,
                      const checked_value<V, C>& checked)
    -> decltype(V(checked) > value)
{
    return V(checked) > value;
}

template <class V, class C>
inline auto operator>=(const checked_value<V, C>& checked,
                       const V& value)
    -> decltype(V(checked) >= value)
{
    return V(checked) >= value;
}

template <class V, class C>
inline auto operator>=(const V& value,
                       const checked_value<V, C>& checked)
    -> decltype(V(checked) >= value)
{
    return V(checked) >= value;
}

template <class T, class U>
auto operator<<(std::ostream& os, const checked_value<T, U>& value)
    -> decltype((os << T(value)), std::declval<std::ostream&>())
{
    os << T(value);
    return os;
}

}

#endif /* restricted_string_h */
