#include "pipe_registry.hpp"

namespace flow {

pipe_registry& the_pipe_registry() noexcept
{
    static auto registry = pipe_registry{};
    return registry;
}

}
