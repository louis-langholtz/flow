#ifndef utility_hpp
#define utility_hpp

#include <map>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <vector>

#include "ext/fstream.hpp"

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/process_id.hpp"

namespace flow {

struct file_port;
struct instance;
struct prototype_name;
struct executable_prototype;
struct system_prototype;

auto temporary_fstream() -> ext::fstream;
auto nulldev_fstream() -> ext::fstream;

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable
/// for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
auto make_arg_bufs(const std::vector<std::string>& strings,
                   const std::string& fallback = {})
    -> std::vector<std::string>;

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable
/// for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
auto make_arg_bufs(const std::map<std::string, std::string>& envars)
    -> std::vector<std::string>;

/// @brief Makes a vector that's compatible for use with <code>execve</code>'s
///   <code>argv</code> parameter.
/// @note This is NOT an "async-signal-safe" function. So, it's not suitable
/// for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
auto make_argv(const std::span<std::string>& args)
    -> std::vector<char*>;

auto write(std::ostream& os, const std::error_code& ec)
    -> std::ostream&;

/// @brief Outputs diagnostics information to the given output stream.
auto write_diags(const prototype_name& name, instance& object,
                 std::ostream& os) -> void;

auto find_index(const std::span<const connection>& connections,
                const connection& look_for)
    -> std::optional<std::size_t>;

auto find_index(const std::span<const connection>& connections,
                const endpoint& look_for) -> std::optional<std::size_t>;

auto touch(const file_port& file) -> void;

auto mkfifo(const file_port& file) -> void;

enum class wait_mode {
    quiet, diagnostic,
};

auto wait(const prototype_name& name, instance& instance,
          std::ostream& diags, wait_mode mode = wait_mode::quiet) -> void;

enum class signal {
    interrupt,
    terminate,
    kill,
};

auto operator<<(std::ostream& os, signal s) -> std::ostream&;

auto send_signal(signal sig,
                 const prototype_name& name,
                 const instance& instance,
                 std::ostream& diags) -> void;

auto set_signal_handler(signal sig) -> void;

}

#endif /* utility_hpp */
