#ifndef restricted_string_h
#define restricted_string_h

#include <string>
#include <type_traits>
#include <utility> // for std::exchange

namespace flow::detail {

template <class T, class R, class ...Args>
concept functor_returns = std::is_invocable_r_v<R, T, Args...>;

template <class T, functor_returns<T, T> Checker>
struct checked_value
{
    using value_type = T;
    using checker_type = Checker;

    template <bool B = functor_returns<Checker, T>, std::enable_if_t<B, int> = 0>
    constexpr checked_value() // NOLINT(bugprone-exception-escape)
    noexcept(noexcept(Checker{}()) && std::is_nothrow_move_constructible_v<T>):
    data{Checker{}()}
    {
        // Intentionally empty.
    }

    template <bool B = functor_returns<Checker, T>,
    std::enable_if_t<!B && std::is_default_constructible_v<T>, int> = 0>
    constexpr checked_value() // NOLINT(bugprone-exception-escape)
    noexcept(noexcept(Checker{}(T{})) && std::is_nothrow_move_constructible_v<T>):
    data{Checker{}(T{})}
    {
        // Intentionally empty.
    }

    checked_value(const checked_value& other) = default;

    checked_value(checked_value&& other) // NOLINT(bugprone-exception-escape)
    noexcept(std::is_nothrow_move_constructible_v<value_type> &&
             std::is_nothrow_assignable_v<value_type,
                decltype(checker_type{}())> &&
             noexcept(Checker{}())):
        data{std::exchange(other.data, checker_type{}())}
    {
        // Intentionally empty.
    }

    template <class U, class V = std::enable_if_t<
        !std::is_same_v<std::decay_t<U>, checked_value> &&
        functor_returns<Checker, T, U>
    >>
    checked_value(U&& u): data{
        checker_type{}(std::forward<U>(u)) // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    }
    {
        // Intentionally empty.
    }

    template<class InputIt, class U = std::enable_if_t<
        functor_returns<Checker, T, InputIt, InputIt>
    >>
    checked_value(InputIt first, InputIt last)
        : data{checker_type{}(first, last)}
    {
        // Intentionally empty.
    }

    auto operator=(const checked_value& other) -> checked_value& = default;

    auto operator=(checked_value&& other) // NOLINT(bugprone-exception-escape)
        noexcept(std::is_nothrow_move_assignable_v<value_type> &&
                 std::is_nothrow_assignable_v<value_type,
                    decltype(checker_type{}())>)
        -> checked_value&
    {
        if (this != &other) {
            data = std::exchange(other.data, checker_type{}());
        }
        return *this;
    }

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

template <class T>
concept ostreamable = requires{
    std::declval<std::ostream&>() << std::declval<T>();
};

template <ostreamable T, class U>
auto operator<<(std::ostream& os, const checked_value<T, U>& value)
    -> std::ostream&
{
    os << value.get();
    return os;
}

}

#endif /* restricted_string_h */
