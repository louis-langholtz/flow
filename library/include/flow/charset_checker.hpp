#ifndef reserved_chars_checker_hpp
#define reserved_chars_checker_hpp

#include <array>
#include <string>

namespace flow::detail {

enum class char_list { deny, allow };

auto charset_validator(std::string v,
                       char_list access,
                       const std::string& chars)
    -> std::string;

template <char... chars>
struct constexpr_ntbs
{
    static constexpr auto data() noexcept
    {
        return std::data(ntbs);
    }

    static constexpr auto size() noexcept
    {
        return std::size(ntbs) - 1u;
    }

    static constexpr auto begin() noexcept
    {
        return std::begin(ntbs);
    }

    static constexpr auto end() noexcept
    {
        return std::end(ntbs) - 1u;
    }

    operator std::string() const
    {
        return std::string{data(), size()};
    }

private: // prevent direct access to underlying implementation...
    static constexpr auto ntbs = std::array<char, sizeof...(chars) + 1u>{
        chars..., '\0'
    };
};

template <typename...> struct char_template_joiner;

template <
    template<char...> class Tpl,
    char ...Args1>
struct char_template_joiner<Tpl<Args1...>>
{
    using type = Tpl<Args1...>;
};

template <
    template<char...> class Tpl,
    char ...Args1,
    char ...Args2>
struct char_template_joiner<Tpl<Args1...>, Tpl<Args2...>>
{
    using type = Tpl<Args1..., Args2...>;
};

template <template <char...> class Tpl,
          char ...Args1,
          char ...Args2,
          typename ...Tail>
struct char_template_joiner<Tpl<Args1...>, Tpl<Args2...>, Tail...>
{
     using type = typename char_template_joiner<Tpl<Args1..., Args2...>, Tail...>::type;
};

template <class T>
concept is_stringable = std::convertible_to<T, std::string>;

static_assert(is_stringable<constexpr_ntbs<>>);

template <is_stringable... Args>
auto concatenate(Args const&... args) -> std::string
{
    // Using a fold expression like: return (std::string{args} + ...);
    // is more succinct, but adding string that way is less inefficient.
    std::string result;
    (void) std::to_array({
        0, (static_cast<void>(result += std::string{args}), 0)...
    });
    return result;
}

template <char_list access, is_stringable... Charsets>
struct charset_checker
{
    static inline const auto charset = concatenate(Charsets{}...);

    auto operator()() const noexcept // NOLINT(bugprone-exception-escape)
        -> std::string
    {
        return {};
    }

    auto operator()(std::string v) const -> std::string
    {
        return charset_validator(std::move(v), access, charset);
    }

    auto operator()(const char *v) const -> std::string
    {
        return operator()(std::string(v));
    }

    template <class InputIt>
    auto operator()(InputIt first, InputIt last) const -> std::string
    {
        return operator()(std::string{first, last});
    }
};

template <is_stringable... Charsets>
using denied_chars_checker = charset_checker<char_list::deny, Charsets...>;

template <is_stringable... Charsets>
using allowed_chars_checker = charset_checker<char_list::allow, Charsets...>;

using upper_charset = constexpr_ntbs<
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'
>;

using lower_charset = constexpr_ntbs<
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z'
>;

using alpha_charset = char_template_joiner<
    upper_charset, lower_charset
>::type;

using digit_charset = constexpr_ntbs<
    '0','1','2','3','4','5','6','7','8','9'
>;

using alphanum_charset = char_template_joiner<
    alpha_charset, digit_charset
>::type;

using name_charset = char_template_joiner<
    alphanum_charset, constexpr_ntbs<'-','_','#'>
>::type;

}

#endif /* reserved_chars_checker_hpp */
