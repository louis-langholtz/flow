#ifndef wait_result_hpp
#define wait_result_hpp

#include <ostream>
#include <vector>

#include "flow/os_error_code.hpp"
#include "flow/reference_process_id.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, plus ostream support
#include "flow/wait_status.hpp"

namespace flow {

struct wait_result {
    enum type_enum: unsigned {no_children = 0u, has_error, has_info};

    struct no_kids_t {
        constexpr auto operator<=>(const no_kids_t&) const noexcept = default;
    };

    using error_t = os_error_code;

    struct info_t {
        reference_process_id id{invalid_process_id};
        wait_status status{wait_unknown_status{}};
    };

    constexpr wait_result() noexcept = default;

    wait_result(no_kids_t) noexcept: wait_result{} {};

    wait_result(error_t error) noexcept: data{error} {}

    wait_result(info_t info) noexcept: data{info} {}

    [[nodiscard]] constexpr auto type() const noexcept -> type_enum
    {
        return static_cast<type_enum>(data.index());
    }

    constexpr explicit operator bool() const noexcept
    {
        return type() != no_children;
    }

    [[nodiscard]] constexpr auto holds_no_kids() const noexcept
    {
        return std::holds_alternative<no_kids_t>(data);
    }

    auto no_kids() const& -> const no_kids_t& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<no_kids_t>(data);
    }

    [[nodiscard]] constexpr auto holds_error() const noexcept
    {
        return std::holds_alternative<error_t>(data);
    }

    auto error() const& -> const error_t& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<error_t>(data);
    }

    [[nodiscard]] constexpr auto holds_info() const noexcept
    {
        return std::holds_alternative<info_t>(data);
    }

    auto info() const& -> const info_t& // NOLINT(modernize-use-nodiscard)
    {
        return std::get<info_t>(data);
    }

private:
    friend constexpr auto operator==(const wait_result& lhs,
                                     const wait_result& rhs) noexcept;

    variant<no_kids_t, error_t, info_t> data;
};

constexpr auto operator==(const wait_result::info_t& lhs,
                          const wait_result::info_t& rhs) noexcept
{
    return (lhs.id == rhs.id) && (lhs.status == rhs.status);
}

constexpr auto operator==(const wait_result& lhs,
                          const wait_result& rhs) noexcept
{
    return lhs.data == rhs.data;
}

auto operator<<(std::ostream& os, const wait_result::no_kids_t&)
    -> std::ostream&;
auto operator<<(std::ostream& os, const wait_result::info_t& value)
    -> std::ostream&;
auto operator<<(std::ostream& os, const wait_result& value) -> std::ostream&;

struct system_name;
struct instance;

enum class wait_option: int;

constexpr auto operator|(const wait_option& lhs,
                         const wait_option& rhs) noexcept
    -> wait_option
{
    return wait_option(int(lhs) | int(rhs));
}

constexpr auto operator&(const wait_option& lhs,
                         const wait_option& rhs) noexcept
    -> wait_option
{
    return wait_option(int(lhs) & int(rhs));
}

constexpr auto operator~(const wait_option& val) noexcept
    -> wait_option
{
    return wait_option(~int(val));
}

namespace detail {

auto get_nohang_wait_option() noexcept -> wait_option;

}

namespace wait_options {

const wait_option nohang = detail::get_nohang_wait_option();

}

auto wait(reference_process_id id = invalid_process_id,
          wait_option flags = {}) noexcept
    -> wait_result;

auto wait(const system_name& name, instance& object)
    -> std::vector<wait_result>;

}

#endif /* wait_result_hpp */
