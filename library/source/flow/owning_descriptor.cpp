#include <unistd.h> // for close

#include <utility> // for std::exchange

#include "flow/owning_descriptor.hpp"

namespace flow {

owning_descriptor::~owning_descriptor()
{
    close();
}

owning_descriptor::owning_descriptor(owning_descriptor&& other) noexcept
    : d{std::exchange(other.d, default_descriptor)} {}

auto owning_descriptor::operator=(owning_descriptor&& other) noexcept
    -> owning_descriptor&
{
    if (&other != this) {
        d = std::exchange(other.d, default_descriptor);
    }
    return *this;
}

auto owning_descriptor::close() noexcept -> os_error_code
{
    if (d != descriptors::invalid_id) {
        if (::close(int(d)) == -1) {
            return os_error_code{errno};
        }
        d = descriptors::invalid_id;
    }
    return os_error_code{};
}

}
