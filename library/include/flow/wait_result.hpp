#ifndef wait_result_hpp
#define wait_result_hpp

#include <ostream>

#include "flow/os_error_code.hpp"
#include "flow/reference_process_id.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, plus ostream support
#include "flow/wait_status.hpp"

namespace flow {

struct wait_result {
    enum type_enum: unsigned {no_children = 0u, has_error, has_info};

    struct no_kids_t {
    };

    using error_t = os_error_code;

    struct info_t {
        enum status_enum: unsigned {unknown, exit, signaled, stopped, continued};
        reference_process_id id{invalid_process_id};
        wait_status status{wait_unknown_status{}};
    };

    wait_result() noexcept = default;

    wait_result(no_kids_t) noexcept: wait_result{} {};

    wait_result(error_t error): data{error} {}

    wait_result(info_t info): data{info} {}

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
    variant<no_kids_t, error_t, info_t> data;
};

auto operator<<(std::ostream& os, const wait_result::no_kids_t&)
    -> std::ostream&;
auto operator<<(std::ostream& os, const wait_result::info_t& value)
    -> std::ostream&;
auto operator<<(std::ostream& os, const wait_result& value) -> std::ostream&;

}

#endif /* wait_result_hpp */
