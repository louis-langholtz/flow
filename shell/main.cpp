#include <iomanip> // for std::quoted
#include <iostream>
#include <iterator>
#include <memory>
#include <string> // for std::getline
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
    std::string word;
    while (std::cin >> std::quoted(word)) {
        // TODO: make this into a shell-like application!
        std::cout << word << "\n";
    }
    return 0;
}
