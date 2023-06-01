#ifndef owning_process_id_hpp
#define owning_process_id_hpp

#include <experimental/propagate_const>
#include <memory> // for std::unique_ptr
#include <type_traits> // for std::is_default_constructible_v

#include "flow/reference_process_id.hpp"
#include "flow/wait_result.hpp"

namespace flow {

struct owning_process_id
{
    static constexpr auto default_process_id = invalid_process_id;

    static auto fork() -> owning_process_id;

    owning_process_id() noexcept;
    owning_process_id(reference_process_id id);
    owning_process_id(const owning_process_id& other) = delete;
    owning_process_id(owning_process_id&& other) noexcept;
    ~owning_process_id();

    auto operator=(const owning_process_id& other) -> owning_process_id& = delete;
    auto operator=(owning_process_id&& other) noexcept -> owning_process_id&;

    operator reference_process_id() const noexcept;

    explicit operator std::int32_t() const
    {
        return std::int32_t(reference_process_id(*this));
    }

    auto wait(wait_option flags = {}) noexcept -> wait_result;

    auto operator<=>(const owning_process_id& other) const noexcept;

private:
    reference_process_id pid{default_process_id};
};

static_assert(std::is_default_constructible_v<owning_process_id>);
static_assert(std::is_move_constructible_v<owning_process_id>);
static_assert(std::is_move_assignable_v<owning_process_id>);
static_assert(!std::is_copy_constructible_v<owning_process_id>);
static_assert(!std::is_copy_assignable_v<owning_process_id>);

}

#endif /* owning_process_id_hpp */
