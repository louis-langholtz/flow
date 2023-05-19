#ifndef checked_hpp
#define checked_hpp

#include <string>
#include <type_traits>
#include <utility> // for std::exchange

namespace flow::detail {

template <class T, class R, class ...Args>
concept functor_returns = std::is_invocable_r_v<R, T, Args...>;

template <class T, functor_returns<T, T> Checker, class X = void>
struct checked
{
    using value_type = T;
    using checker_type = Checker;

    template <bool B = functor_returns<Checker, T>, std::enable_if_t<B, int> = 0>
    constexpr checked() // NOLINT(bugprone-exception-escape)
    noexcept(noexcept(Checker{}()) && std::is_nothrow_move_constructible_v<T>):
    data{Checker{}()}
    {
        // Intentionally empty.
    }

    template <bool B = functor_returns<Checker, T>,
    std::enable_if_t<!B && std::is_default_constructible_v<T>, int> = 0>
    constexpr checked() // NOLINT(bugprone-exception-escape)
    noexcept(noexcept(Checker{}(T{})) && std::is_nothrow_move_constructible_v<T>):
    data{Checker{}(T{})}
    {
        // Intentionally empty.
    }

    checked(const checked& other) = default;

    checked(checked&& other) // NOLINT(bugprone-exception-escape)
    noexcept(std::is_nothrow_move_constructible_v<value_type> &&
             std::is_nothrow_assignable_v<value_type,
                decltype(checker_type{}())> &&
             noexcept(Checker{}())):
        data{std::exchange(other.data, checker_type{}())}
    {
        // Intentionally empty.
    }

    template <class U, class V = std::enable_if_t<
        !std::is_same_v<std::decay_t<U>, checked> &&
        functor_returns<Checker, T, U>
    >>
    checked(U&& u): data{
        checker_type{}(std::forward<U>(u)) // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    }
    {
        // Intentionally empty.
    }

    template<class InputIt, class U = std::enable_if_t<
        std::is_invocable_r_v<T, Checker, InputIt, InputIt>
    >>
    checked(InputIt first, InputIt last)
        : data{checker_type{}(first, last)}
    {
        // Intentionally empty.
    }

    auto operator=(const checked& other) -> checked& = default;

    auto operator=(checked&& other) // NOLINT(bugprone-exception-escape)
        noexcept(std::is_nothrow_move_assignable_v<value_type> &&
                 std::is_nothrow_assignable_v<value_type,
                    decltype(checker_type{}())>)
        -> checked&
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

template <class LhsV, class LhsC, class LhsX,
          class RhsV, class RhsC, class RhsX>
inline auto operator==(const checked<LhsV, LhsC, LhsX>& lhs,
                       const checked<RhsV, RhsC, RhsX>& rhs)
    -> decltype(LhsV(lhs) == RhsV(rhs))
{
    return LhsV(lhs) == RhsV(rhs);
}

template <class LhsV, class LhsC, class LhsX,
          class RhsV, class RhsC, class RhsX>
inline auto operator<(const checked<LhsV, LhsC, LhsX>& lhs,
                      const checked<RhsV, RhsC, RhsX>& rhs)
    -> decltype(LhsV(lhs) < RhsV(rhs))
{
    return LhsV(lhs) < RhsV(rhs);
}

template <class V, class C, class X>
inline auto operator==(const checked<V, C, X>& checked,
                       const V& value)
    -> decltype(V(checked) == value)
{
    return V(checked) == value;
}

template <class V, class C, class X>
inline auto operator==(const V& value,
                       const checked<V, C, X>& checked)
    -> decltype(V(checked) == value)
{
    return V(checked) == value;
}

template <class V, class C, class X>
inline auto operator<(const checked<V, C, X>& checked,
                      const V& value)
    -> decltype(V(checked) < value)
{
    return V(checked) < value;
}

template <class V, class C, class X>
inline auto operator<(const V& value,
                      const checked<V, C, X>& checked)
    -> decltype(V(checked) == value)
{
    return V(checked) == value;
}

template <class V, class C, class X>
inline auto operator<=(const checked<V, C, X>& checked,
                       const V& value)
    -> decltype(V(checked) <= value)
{
    return V(checked) <= value;
}

template <class V, class C, class X>
inline auto operator<=(const V& value,
                       const checked<V, C, X>& checked)
    -> decltype(V(checked) <= value)
{
    return V(checked) == value;
}

template <class V, class C, class X>
inline auto operator>(const checked<V, C, X>& checked,
                      const V& value)
    -> decltype(V(checked) > value)
{
    return V(checked) > value;
}

template <class V, class C, class X>
inline auto operator>(const V& value,
                      const checked<V, C, X>& checked)
    -> decltype(V(checked) > value)
{
    return V(checked) > value;
}

template <class V, class C, class X>
inline auto operator>=(const checked<V, C, X>& checked,
                       const V& value)
    -> decltype(V(checked) >= value)
{
    return V(checked) >= value;
}

template <class V, class C, class X>
inline auto operator>=(const V& value,
                       const checked<V, C, X>& checked)
    -> decltype(V(checked) >= value)
{
    return V(checked) >= value;
}

template <class T>
concept ostreamable = requires{
    std::declval<std::ostream&>() << std::declval<T>();
};

template <ostreamable T, class U, class X>
auto operator<<(std::ostream& os, const checked<T, U, X>& value)
    -> std::ostream&
{
    os << value.get();
    return os;
}

}

#endif /* checked_hpp */
