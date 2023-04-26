#ifndef utility_hpp
#define utility_hpp

#include <map>
#include <ostream>
#include <span>
#include <string>
#include <vector>

namespace flow {

struct instance;
struct prototype_name;

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
std::vector<std::string> make_arg_bufs(const std::vector<std::string>& strings,
                                       const std::string& fallback = {});

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
std::vector<std::string> make_arg_bufs(const std::map<std::string, std::string>& envars);

/// @brief Makes a vector that's compatible for use with <code>execve</code>'s <code>argv</code> parameter.
/// @note This is NOT an "async-signal-safe" function. So, it's not suitable for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
std::vector<char*> make_argv(const std::span<std::string>& args);

void show_diags(const prototype_name& name, instance& object, std::ostream& os);

}

#endif /* utility_hpp */
