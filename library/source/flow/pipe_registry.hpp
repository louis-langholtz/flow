#ifndef pipe_registry_hpp
#define pipe_registry_hpp

#include <set>

namespace flow {

struct pipe_channel;

struct pipe_registry
{
    std::set<pipe_channel*> pipes;
};

pipe_registry& the_pipe_registry() noexcept;

}

#endif /* pipe_registry_hpp */
