#ifndef owning_process_id_hpp
#define owning_process_id_hpp

#include <experimental/propagate_const>
#include <memory> // for std::unique_ptr
#include <type_traits> // for std::is_default_constructible_v

#include "flow/reference_process_id.hpp"
#include "flow/wait_result.hpp"

namespace flow {

/// @brief Owning process identifier.
/// @note Provides some RAII-styled ownership handling for spawning processes.
/// @note Implementation of this was based on POSIX process handling.
///   There has been at least one proposal for bring process handling into the
///   the C++ standard which may provide ideas for improving this code.
/// @todo Consider C++ standards proposals for updating this code.
/// @see https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1750r1.pdf.
struct owning_process_id
{
    struct impl;

    static constexpr auto default_process_id = invalid_process_id;
    static constexpr auto default_status = wait_unknown_status{};

    static auto fork() -> reference_process_id;

    owning_process_id();
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

    auto wait(wait_option flags = {}) noexcept -> wait_status;

    auto operator<=>(const owning_process_id& other) const noexcept;

    /// @brief Status of the process.
    /// @note This is an observer function.
    /// @return <code>wait_unknown_status{}</code> if the associated process
    ///   has not yet terminated (possibly because this has no associated
    ///   process).
    [[nodiscard]] auto status() const noexcept -> wait_status;

private:
    std::experimental::propagate_const<std::unique_ptr<impl>> pimpl;
};

static_assert(std::is_default_constructible_v<owning_process_id>);
static_assert(std::is_move_constructible_v<owning_process_id>);
static_assert(std::is_move_assignable_v<owning_process_id>);
static_assert(!std::is_copy_constructible_v<owning_process_id>);
static_assert(!std::is_copy_assignable_v<owning_process_id>);

}

#endif /* owning_process_id_hpp */
