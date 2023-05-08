#ifndef pipe_registry_hpp
#define pipe_registry_hpp

#include <set>

namespace flow {

struct pipe_channel;

struct pipe_registry
{
    std::set<pipe_channel*> pipes;
};

auto the_pipe_registry() noexcept -> pipe_registry&;

}

#endif /* pipe_registry_hpp */
