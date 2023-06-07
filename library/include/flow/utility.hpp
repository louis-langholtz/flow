#ifndef utility_hpp
#define utility_hpp

#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ext/fstream.hpp"

#include "flow/connection.hpp"
#include "flow/port_map.hpp"
#include "flow/environment_map.hpp"
#include "flow/signal.hpp"
#include "flow/system_name.hpp"

namespace flow {

namespace detail {
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}

struct file_endpoint;
struct instance;

auto nulldev_fstream() -> ext::fstream;

/// @note This is NOT an "async-signal-safe" function. So, it's not suitable
/// for forked child to call.
/// @see https://man7.org/linux/man-pages/man7/signal-safety.7.html
auto make_arg_bufs(const std::vector<std::string>& strings,
                   const std::string& fallback = {})
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
auto write_diags(instance& object,
                 std::ostream& os,
                 const std::string_view& name = {}) -> void;

auto find_index(const std::span<const connection>& connections,
                const connection& look_for)
    -> std::optional<std::size_t>;

auto find_index(const std::span<const connection>& connections,
                const endpoint& look_for) -> std::optional<std::size_t>;

auto touch(const file_endpoint& file) -> void;

auto mkfifo(const file_endpoint& file) -> void;

auto send_signal(signal sig,
                 const instance& instance,
                 std::ostream& diags,
                 const std::string& name = {}) -> void;

auto set_signal_handler(signal sig) -> void;

auto sigsafe_counter_reset() noexcept -> void;
auto sigsafe_counter_take() noexcept -> bool;
auto sigsafe_sigset_put(int sig) noexcept -> bool;
auto sigsafe_sigset_take(int sig) noexcept -> bool;
auto sigsafe_sigset_takeany(const std::set<int>& sigs) -> bool;

auto get_matching_set(const port_map& ports, io_type io)
    -> std::set<port_id>;

/// @brief Converts the given enumerate into its underlying value.
/// @note This is basically a back port from C++23.
template <class Enum>
constexpr auto to_underlying(Enum e) noexcept ->
    decltype(static_cast<std::underlying_type_t<Enum>>(e))
{
    return static_cast<std::underlying_type_t<Enum>>(e);
}

}

#endif /* utility_hpp */
