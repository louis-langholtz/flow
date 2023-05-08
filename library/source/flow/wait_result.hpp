#ifndef wait_result_hpp
#define wait_result_hpp

#include "os_error_code.hpp"

#include "flow/process_id.hpp"
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

    wait_result(error_t error): value{error} {}

    wait_result(info_t info): value{info} {}

    constexpr auto type() const noexcept -> type_enum
    {
        return static_cast<type_enum>(value.index());
    }

    constexpr explicit operator bool() const noexcept
    {
        return type() != no_children;
    }

    auto error() const& -> const error_t&
    {
        return std::get<error_t>(value);
    }

    auto info() const& -> const info_t&
    {
        return std::get<info_t>(value);
    }

private:
    variant<no_kids_t, error_t, info_t> value;
};

}

#endif /* wait_result_hpp */
