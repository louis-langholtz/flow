#ifndef env_name_hpp
#define env_name_hpp

#include <ostream>
#include <string>

namespace flow {

struct env_name
{
    static const std::string reserved_set;

    /// @throws std::invalid_argument If character found from reserved set.
    /// @see reserved_set.
    static std::string validate(std::string v);

    constexpr env_name() noexcept = default;

    /// @throws std::invalid_argument If character found from reserved set.
    /// @see reserved_set.
    env_name(const char *v)
        : data{validate(v)}
    {
        // Intentionally empty.
    }

    template<class InputIt>
    env_name(InputIt first, InputIt last)
        : data{validate(std::string{first, last})}
    {
        // Intentionally empty.
    }

    constexpr explicit operator std::string() const
    {
        return data;
    }

private:
    std::string data;
};

constexpr auto operator==(const env_name& lhs, const env_name& rhs)
    -> bool
{
    return std::string(lhs) == std::string(rhs);
}

constexpr auto operator<(const env_name& lhs, const env_name& rhs)
    -> bool
{
    return std::string(lhs) < std::string(rhs);
}

auto operator<<(std::ostream& os, const env_name& name)
    -> std::ostream&;

}

#endif /* env_name_hpp */
