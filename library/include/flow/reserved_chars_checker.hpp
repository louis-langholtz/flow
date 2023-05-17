#ifndef reserved_chars_checker_hpp
#define reserved_chars_checker_hpp

#include <string>

namespace flow::detail {

auto reserved_chars_validator(std::string v,
                              const std::string& reserved_chars)
    -> std::string;

template <char... chars>
struct reserved_chars_checker
{
    static inline const auto reserved_chars = std::string{chars...};

    auto operator()() const noexcept // NOLINT(bugprone-exception-escape)
        -> std::string
    {
        return {};
    }

    auto operator()(std::string v) const -> std::string
    {
        return reserved_chars_validator(std::move(v), reserved_chars);
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

}

#endif /* reserved_chars_checker_hpp */
