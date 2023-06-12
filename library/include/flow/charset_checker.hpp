#ifndef reserved_chars_checker_hpp
#define reserved_chars_checker_hpp

#include <array>
#include <stdexcept> // for std::invalid_argument
#include <string>
#include <string_view>

#include "flow/tcstring.hpp"

namespace flow {

enum class char_list { deny, allow };

struct charset_validator_error: public std::invalid_argument
{
    using invalid_argument::invalid_argument;

    charset_validator_error(char badc,
                            std::string chars,
                            char_list acc,
                            const std::string& what_arg = {});

    [[nodiscard]] auto badchar() const noexcept -> char;
    [[nodiscard]] auto access() const noexcept -> char_list;
    [[nodiscard]] auto charset() const -> std::string;

private:
    std::string chars_;
    char_list access_{char_list::deny};
    char badchar_{};
};

}

namespace flow::detail {

/// @brief Character set validator function.
/// @param[in] v Value to validate.
/// @param[in] access Whether validation is to deny or allow finding of a
///   charactaer from @chars.
/// @param[in] chars Characters which @v should be assessed for having or not.
/// @throws charset_validator_error if @v is invalid.
auto charset_validator(std::string v,
                       char_list access,
                       const std::string& chars)
    -> std::string;

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

static_assert(is_stringable<tcstring<>>);

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

    auto operator()(const std::string_view& v) const -> std::string
    {
        return operator()(std::string(v));
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

using upper_charset = tcstring<
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'
>;

using lower_charset = tcstring<
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z'
>;

using alpha_charset = char_template_joiner<
    upper_charset, lower_charset
>::type;

using digit_charset = tcstring<
    '0','1','2','3','4','5','6','7','8','9'
>;

using alphanum_charset = char_template_joiner<
    alpha_charset, digit_charset
>::type;

using name_charset = char_template_joiner<
    alphanum_charset, tcstring<'_'>
>::type;

}

#endif /* reserved_chars_checker_hpp */
