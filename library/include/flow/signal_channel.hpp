#ifndef signal_channel_hpp
#define signal_channel_hpp

#include <ostream>
#include <set>

#include "flow/signal.hpp"
#include "flow/system_name.hpp"

namespace flow {

struct signal_channel
{
    std::set<signal> signals{};
    system_name address;
};

auto operator<<(std::ostream& os, const signal_channel& value)
    -> std::ostream&;

}

#endif /* signal_channel_hpp */
