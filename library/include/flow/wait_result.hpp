#ifndef wait_result_hpp
#define wait_result_hpp

#include <concepts> // for std::regular.
#include <ostream>
#include <vector>

#include "flow/os_error_code.hpp"
#include "flow/reference_process_id.hpp"
#include "flow/variant.hpp" // for <variant>, flow::variant, plus ostream support
#include "flow/wait_option.hpp"
#include "flow/wait_status.hpp"

namespace flow {

struct empty_wait_result {
    constexpr auto operator<=>(const empty_wait_result&) const noexcept =
        default;
};

auto operator<<(std::ostream& os, const empty_wait_result&)
    -> std::ostream&;

struct nokids_wait_result {
    constexpr auto operator<=>(const nokids_wait_result&) const noexcept =
        default;
};

auto operator<<(std::ostream& os, const nokids_wait_result&)
    -> std::ostream&;

struct error_wait_result {
    os_error_code data;
    constexpr auto operator<=>(const error_wait_result&) const noexcept =
        default;
};

auto operator<<(std::ostream& os, const error_wait_result& arg)
    -> std::ostream&;

struct info_wait_result {
    reference_process_id id{invalid_process_id};
    wait_status status{wait_unknown_status{}};
};

constexpr auto operator==(const info_wait_result& lhs,
                          const info_wait_result& rhs) noexcept
{
    return (lhs.id == rhs.id) && (lhs.status == rhs.status);
}

auto operator<<(std::ostream& os, const info_wait_result& arg)
    -> std::ostream&;

using wait_result = variant<
    empty_wait_result,
    nokids_wait_result,
    error_wait_result,
    info_wait_result
>;

static_assert(std::regular<wait_result>);

struct instance;

auto wait(reference_process_id id = invalid_process_id,
          wait_option flags = {}) noexcept
    -> wait_result;

auto wait(instance& object) -> std::vector<wait_result>;

}

#endif /* wait_result_hpp */
