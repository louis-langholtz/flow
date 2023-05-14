#include <iostream>
#include <iterator>
#include <memory>
#include <string_view>
#include <type_traits>

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/indenting_ostreambuf.hpp"
#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/system_name.hpp"
#include "flow/system_pipeline.hpp"
#include "flow/utility.hpp"
#include "flow/wait_result.hpp"

auto main(int argc, const char * argv[]) -> int
{
    using namespace flow;
    set_signal_handler(signal::interrupt);
    set_signal_handler(signal::terminate);
    return 0;
}
