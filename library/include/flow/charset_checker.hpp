#ifndef reserved_chars_checker_hpp
#define reserved_chars_checker_hpp

#include <string>

namespace flow::detail {

enum class char_list { deny, allow };

auto charset_validator(std::string v,
                       char_list access,
                       const std::string& chars)
    -> std::string;

template <char_list access, char... chars>
struct charset_checker
{
    static inline const auto charset = std::string{chars...};

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

template <char... chars>
using denied_chars_checker = charset_checker<char_list::deny, chars...>;

template <char... chars>
using allowed_chars_checker = charset_checker<char_list::allow, chars...>;

using name_chars_checker = allowed_chars_checker<
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','-','_','+'
>;

}

#endif /* reserved_chars_checker_hpp */
