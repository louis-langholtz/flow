#ifndef expected_h
#define expected_h

#ifdef __has_include
# if __has_include(<version>)
#   include <version>
# endif
#endif

#if !defined(__has_include) || !__has_include(<expected>) \
    || !defined(__cplusplus) || (__cplusplus < 202300L)

#include <initializer_list>
#include <variant> // for std::get, std::get_if
#include <type_traits> // for std::is_convertible_v
#include <utility> // for std::in_place_t

namespace flow {

template<class E>
struct unexpected {
    constexpr unexpected() = default;

    constexpr unexpected(const unexpected&) = default;

    constexpr unexpected(unexpected&&)
        noexcept(noexcept(std::is_nothrow_move_constructible_v<E>)) = default;

    template<class Err = E, class X = std::enable_if_t<
        !std::is_same_v<std::remove_cvref_t<Err>, unexpected> &&
        std::is_constructible_v<E, Err>, E>>
    constexpr explicit unexpected(Err&& err):
        val{std::forward<Err>(err)} // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    {
        // Intentionally empty.
    }

    constexpr auto operator=(const unexpected&) -> unexpected& = default;

    constexpr auto operator=(unexpected&&)
        noexcept(noexcept(std::is_nothrow_move_constructible_v<E>))
        -> unexpected& = default;

    [[nodiscard]] constexpr auto value() const& noexcept -> const E&
    {
        return val;
    }

    [[nodiscard]] constexpr auto value() & noexcept -> E&
    {
        return val;
    }

    [[nodiscard]] constexpr auto value() const&& noexcept -> const E&&
    {
        return val;
    }

    [[nodiscard]] constexpr auto value() && noexcept -> E&&
    {
        return val;
    }

    template<class E2>
    friend constexpr auto
    operator==(const unexpected&, const unexpected<E2>&) -> bool;

private:
    E val;
};

/// @brief An implementation of <code>std::expected</code> for C++20.
/// @see https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0323r11.html
template <class T, class E>
struct expected {
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    static_assert(std::is_default_constructible_v<T>);

    constexpr expected() = default;

    template<class U = T, class X = std::enable_if_t<
        !std::is_same_v<std::remove_cvref_t<U>, expected> &&
        std::is_constructible_v<T, U>, E>>
    constexpr explicit(!std::is_convertible_v<U, T>) expected(U&& v):
        m_value{std::forward<U>(v)}
    {
        // Intentionally empty.
    }

    template<class G>
    constexpr expected(const unexpected<G>& error):
        m_value{error.value()}
    {
        // Intentionall empty.
    }

    template<class G>
    constexpr expected(unexpected<G>&& other):
        m_value{std::move(other.value())}
    {
        // Intentionally empty.
    }

    constexpr auto operator*() const& noexcept -> const T&
    {
        return *std::get_if<T>(&m_value);
    }

    constexpr auto operator*() & noexcept -> T&
    {
        return *std::get_if<T>(&m_value);
    }

    constexpr auto operator*() const&& noexcept -> const T&&
    {
        return *std::get_if<T>(&m_value);
    }

    constexpr auto operator*() && noexcept -> T&&
    {
        return *std::get_if<T>(&m_value);
    }

    constexpr auto operator->() const noexcept -> const T*
    {
        return std::get_if<T>(&m_value);
    }

    constexpr auto operator->() noexcept -> T*
    {
        return std::get_if<T>(&m_value);
    }

    constexpr explicit operator bool() const noexcept
    {
        return m_value.index() == 0u;
    }

    [[nodiscard]] constexpr auto has_value() const noexcept -> bool
    {
        return m_value.index() == 0u;
    }

    constexpr auto value() const& -> const T& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<T>(m_value);
    }

    constexpr auto value() & -> T& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<T>(m_value);
    }

    constexpr auto value() const&& -> const T&& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<T>(m_value);
    }

    constexpr auto value() && -> T&& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<T>(m_value);
    }

    constexpr auto error() const& -> const E& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<E>(m_value);
    }

    constexpr auto error() & -> E& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<E>(m_value);
    }

    constexpr auto error() const&& -> const E&& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<E>(m_value);
    }

    constexpr auto error() && -> E&& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<E>(m_value);
    }

private:
    std::variant<T, E> m_value{T{}};
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
