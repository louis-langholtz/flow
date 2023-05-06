#ifndef environment_container_hpp
#define environment_container_hpp

#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

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

    constexpr const std::string& value() const noexcept
    {
        return data;
    }

private:
    std::string data;
};

constexpr auto operator==(const env_name& lhs, const env_name& rhs)
    -> bool
{
    return lhs.value() == rhs.value();
}

constexpr auto operator<(const env_name& lhs, const env_name& rhs)
    -> bool
{
    return lhs.value() < rhs.value();
}

auto operator<<(std::ostream& os, const env_name& name)
    -> std::ostream&;

using environment_container = std::map<env_name, std::string>;

auto operator<<(std::ostream& os, const environment_container& value)
    -> std::ostream&;

auto get_environ() -> environment_container;

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable
/// for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
auto make_arg_bufs(const std::map<env_name, std::string>& envars)
    -> std::vector<std::string>;

}

#endif /* environment_container_hpp */
