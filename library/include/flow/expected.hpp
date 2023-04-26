#ifndef expected_h
#define expected_h

#if !defined(__has_include) || !__has_include(<expected>)

#include <initializer_list>
#include <variant>
#include <type_traits> // for std::is_convertible_v
#include <utility> // for std::in_place_t

namespace flow {

template<class E>
class unexpected {
public:
    constexpr unexpected() = default;

    constexpr unexpected(const unexpected&) = default;

    constexpr unexpected(unexpected&&)
        noexcept(noexcept(std::is_nothrow_move_constructible_v<E>)) = default;

    template<class Err = E, class X = std::enable_if_t<
        !std::is_same_v<std::remove_cvref_t<Err>, unexpected> &&
        std::is_constructible_v<E, Err>, E>>
    constexpr explicit unexpected(Err&& err): val{std::forward<Err>(err)}
    {
        // Intentionally empty.
    }

    constexpr unexpected& operator=(const unexpected&) = default;

    constexpr unexpected& operator=(unexpected&&)
        noexcept(noexcept(std::is_nothrow_move_constructible_v<E>)) = default;

    constexpr const E& value() const& noexcept
    {
        return val;
    }

    constexpr E& value() & noexcept
    {
        return val;
    }

    constexpr const E&& value() const&& noexcept
    {
        return val;
    }

    constexpr E&& value() && noexcept
    {
        return val;
    }

    template<class E2>
    friend constexpr bool
    operator==(const unexpected&, const unexpected<E2>&);

private:
    E val;
};

/// @brief An implementation of <code>std::expected</code> for C++20.
/// @see https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0323r11.html
template <class T, class E>
class expected {
public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    static_assert(std::is_default_constructible_v<T>);

    constexpr expected() = default;

    template<class U = T, class X = std::enable_if_t<
        !std::is_same_v<std::remove_cvref_t<U>, expected> &&
        std::is_constructible_v<T, U>, E>>
    constexpr explicit(!std::is_convertible_v<U, T>) expected(U&& v):
        value{std::forward<U>(v)}
    {
        // Intentionally empty.
    }

    template<class G>
    constexpr expected(const unexpected<G>& error):
        value{error.value()}
    {
        // Intentionall empty.
    }

    template<class G>
    constexpr expected(unexpected<G>&& other):
        value{std::move(other.value())}
    {
        // Intentionally empty.
    }

    constexpr bool has_value() const noexcept
    {
        return value.index() == 0u;
    }

private:
    std::variant<T, E> value{T{}};
};

}

#else // presumably compiler supporting C++23

#include <expected>

namespace flow {
using std::unexpected;
using std::expected;
}

#endif

#endif /* expected_h */
