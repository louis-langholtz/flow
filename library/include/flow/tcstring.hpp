#ifndef tcstring_hpp
#define tcstring_hpp

#include <array>
#include <string>
#include <string_view>

namespace flow::detail {

template <char... chars>
struct tcstring
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

    constexpr operator std::string_view() const noexcept
    {
        return std::string_view{data(), size()};
    }

private: // prevent direct access to underlying implementation...
    static constexpr auto ntbs = std::array<char, sizeof...(chars) + 1u>{
        chars..., '\0'
    };
};

}

#endif /* tcstring_hpp */
