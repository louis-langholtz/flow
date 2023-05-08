#include "pipe_registry.hpp"

namespace flow {

auto the_pipe_registry() noexcept -> pipe_registry&
{
    static auto registry = pipe_registry{};
    return registry;
}

}
