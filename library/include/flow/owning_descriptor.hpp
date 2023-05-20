#ifndef owning_descriptor_hpp
#define owning_descriptor_hpp

#include <type_traits> // for std::is_default_constructible_v

#include "flow/os_error_code.hpp"
#include "flow/reference_descriptor.hpp"

namespace flow {

struct owning_descriptor
{
    static constexpr auto default_descriptor = descriptors::invalid_id;

    owning_descriptor() noexcept = default;
    owning_descriptor(reference_descriptor d_) noexcept: d{d_} {}
    owning_descriptor(int d_) noexcept: d{reference_descriptor(d_)} {}
    owning_descriptor(owning_descriptor&& other) noexcept;
    owning_descriptor(const owning_descriptor& other) = delete;
    ~owning_descriptor();

    auto operator=(owning_descriptor&& other) noexcept -> owning_descriptor&;
    auto operator=(const owning_descriptor& other) noexcept = delete;

    operator reference_descriptor() const noexcept { return d; }
    explicit operator int() const noexcept { return int(d); }

    explicit operator bool() const noexcept { return d != default_descriptor; }

    auto close() noexcept -> os_error_code;

private:
    reference_descriptor d{default_descriptor};
};

static_assert(std::is_default_constructible_v<owning_descriptor>);
static_assert(std::is_move_constructible_v<owning_descriptor>);
static_assert(std::is_move_assignable_v<owning_descriptor>);
static_assert(!std::is_copy_constructible_v<owning_descriptor>);
static_assert(!std::is_copy_assignable_v<owning_descriptor>);

}

#endif /* owning_descriptor_hpp */
