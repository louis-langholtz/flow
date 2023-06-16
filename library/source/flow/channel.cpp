#include <cerrno> // for errno
#include <cstring> // for std::streror
#include <iostream>
#include <optional>
#include <sstream> // for std::ostringstream

#include <fcntl.h> // for ::open

#include "flow/channel.hpp"
#include "flow/instance.hpp"
#include "flow/invalid_link.hpp"
#include "flow/node.hpp"
#include "flow/utility.hpp"

namespace flow {

namespace {

enum class port_type { unknown, descriptor, signal };

auto at(const port_map& ports, const port_id& key, const node_name& name)
    -> const port_info&
{
    try {
        return ports.at(key);
    }
    catch (const std::out_of_range& ex) {
        std::ostringstream os;
        os << "can't find " << key << " in ";
        if (name == node_name{}) {
            os << "system's";
        }
        else {
            os << name << " node's";
        }
        os << " descriptor mapping: ";
        os << ex.what();
        throw std::invalid_argument{os.str()};
    }
}

using port_size_array = std::array<std::size_t, std::variant_size_v<port_id>>;

auto count_types(const std::set<port_id>& ports)
    -> port_size_array
{
    auto counts = port_size_array{};
    for (auto&& port: ports) {
        ++counts[port.index()];
    }
    return counts;
}

auto multiple_types(const port_size_array& counts) -> bool
{
    auto previous = false;
    for (auto&& count: counts) {
        if (count > 0u) {
            if (previous) {
                return true;
            }
            previous = true;
        }
    }
    return false;
}

/// @brief Gets the index in the variant of the specified type.
/// @see https://stackoverflow.com/a/52304395/7410358
template <typename T, typename... Ts>
constexpr auto get_index(std::variant<Ts...> const&) -> std::size_t
{
    auto r = std::size_t{};
    auto test = [&](bool b){
        if (!b) {
            ++r;
        }
        return b;
    };
    (test(std::is_same_v<T,Ts>) || ...);
    return r;
}

auto get_port_type(const port_size_array& counts) -> port_type
{
    for (auto&& count: counts) {
        const auto index = static_cast<std::size_t>(&count - data(counts));
        if (count > 0u) {
            if (get_index<signal>(port_id{}) == index) {
                return port_type::signal;
            }
            if (get_index<reference_descriptor>(port_id{}) == index) {
                return port_type::descriptor;
            }
        }
    }
    return port_type::unknown;
}

auto validate(const node_endpoint& end,
              const node& node,
              io_type expected_io) -> port_type
{
    std::ostringstream os;
    if (end.address == node_name{}) {
        auto prefix = "bad interface-node endpoint io: ";
        for (auto&& d: end.ports) {
            const auto& d_info = at(node.interface, d, end.address);
            if (d_info.direction != reverse(expected_io)) {
                os << prefix << "expected ";
                prefix = "; ";
                os << reverse(expected_io);
                os << " for " << d;
                os << ", got ";
                os << d_info.direction;
            }
        }
        if (const auto err = os.str(); !empty(err)) {
            throw std::invalid_argument{err};
        }
        const auto counts = count_types(end.ports);
        if (multiple_types(counts)) {
            os << "node endpoint can't specify";
            os << " both signal & descriptor ports";
            throw std::invalid_argument{os.str()};
        }
        return get_port_type(counts);
    }
    if (const auto p = std::get_if<system>(&(node.implementation))) {
        const auto found = p->nodes.find(end.address);
        if (found == p->nodes.end()) {
            os << "endpoint addressed node ";
            os << end.address;
            os << " not found";
            throw std::invalid_argument{os.str()};
        }
        const auto& subsys = found->second;
        auto prefix = "bad internal-node endpoint io: ";
        for (auto&& d: end.ports) {
            const auto& d_info = at(subsys.interface, d, end.address);
            if (d_info.direction != expected_io) {
                os << prefix << "expected ";
                prefix = "; ";
                os << expected_io;
                os << " for " << d;
                os << ", got ";
                os << d_info.direction;
            }
        }
        if (const auto err = os.str(); !empty(err)) {
            throw std::invalid_argument{err};
        }
        const auto counts = count_types(end.ports);
        if (multiple_types(counts)) {
            os << "node endpoint can't specify";
            os << " both signal & descriptor ports";
            throw std::invalid_argument{os.str()};
        }
        return get_port_type(counts);
    }
    if (const auto p = std::get_if<executable>(&(node.implementation))) {
        return port_type::unknown;
    }
    throw std::logic_error{"unknown node implementation type"};
}

auto make_forwarding_channel(const file_endpoint& src, const file_endpoint& dst)
    -> forwarding_channel
{
    const auto mode = 0600;
    auto src_d = owning_descriptor{
        ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
               src.path.c_str(), O_RDONLY, mode)
    };
    if (!src_d) {
        const auto err = os_error_code(errno);
        std::ostringstream os;
        os << "can't open source file endpoint " << src.path;
        os << ": " << err;
        throw std::invalid_argument{os.str()};
    }
    auto dst_d = owning_descriptor{
        ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
               dst.path.c_str(), O_WRONLY, mode)
    };
    if (!dst_d) {
        const auto err = os_error_code(errno);
        std::ostringstream os;
        os << "can't open destination file endpoint " << dst.path;
        os << ": " << err;
        throw std::invalid_argument{os.str()};
    }
    return {std::move(src_d), std::move(dst_d)};
}

auto make_forwarding_channel(const pipe_channel& src, const pipe_channel& dst)
    -> forwarding_channel
{
    return {
        src.get(pipe_channel::io::read),
        dst.get(pipe_channel::io::write)
    };
}

auto make_reference_channel(const std::set<port_id>& dset,
                            const node_name& name,
                            const std::span<const link>& parent_links,
                            const std::span<channel>& parent_channels)
    -> reference_channel
{
    const auto look_for = node_endpoint{name, dset};
    const auto found = find_index(parent_links, look_for);
    if (!found) {
        std::ostringstream os;
        os << "can't find parent link with ";
        os << look_for;
        os << " endpoint for making reference channel";
        throw std::invalid_argument{os.str()};
    }
    return {&parent_channels[*found]};
}

auto make_forwarding_channel(const user_endpoint& src,
                             const user_endpoint& dst,
                             const std::span<const link>& links,
                             const std::span<channel>& channels)
    -> forwarding_channel
{
    const auto src_conn = find_index(links, src);
    if (!src_conn) {
        std::ostringstream os;
        os << "can't find source link with endpoint ";
        os << src;
        throw std::invalid_argument{os.str()};
    }
    const auto dst_conn = find_index(links, dst);
    if (!dst_conn) {
        std::ostringstream os;
        os << "can't find destination link with endpoint ";
        os << dst;
        throw std::invalid_argument{os.str()};
    }
    return make_forwarding_channel(std::get<pipe_channel>(channels[*src_conn]),
                                   std::get<pipe_channel>(channels[*dst_conn]));
}

auto to_signal_set(const std::set<port_id>& ports) -> std::set<signal>
{
    auto signals = std::set<signal>{};
    for (auto&& port: ports) {
        signals.insert(std::get<signal>(port));
    }
    return signals;
}

auto make_signal_channel(const node_endpoint& src,
                         const node_endpoint& dst) -> signal_channel
{
    if (src.ports != dst.ports) {
        std::ostringstream os;
        os << "link between different signal sets not supported";
        throw std::invalid_argument{os.str()};
    }
    if (src.address != node_name{}) {
        std::ostringstream os;
        os << "link src node endpoint for signal(s) must be";
        os << " empty address; not ";
        os << src.address;
        throw std::invalid_argument{os.str()};
    }
    return signal_channel{
        to_signal_set(src.ports),
        dst.address
    };
}

auto get_interface_ports(const node_endpoint* end)
    -> const std::set<port_id>*
{
    return (end && (end->address == node_name{}))? &(end->ports): nullptr;
}

auto make_channel(const endpoint& src,
                  const endpoint& dst,
                  const node_name& name,
                  const node& node,
                  const std::span<channel>& channels,
                  const std::span<const link>& parent_links,
                  const std::span<channel>& parent_channels)
    -> channel
{
    if (src == dst) {
        throw std::invalid_argument{"must have different endpoints"};
    }
    const auto src_file = std::get_if<file_endpoint>(&src);
    const auto dst_file = std::get_if<file_endpoint>(&dst);
    if (src_file && dst_file) {
        return make_forwarding_channel(*src_file, *dst_file);
    }
    const auto src_user = std::get_if<user_endpoint>(&src);
    const auto dst_user = std::get_if<user_endpoint>(&dst);
    if (src_user && dst_user) {
        const auto& links = std::get<system>(node.implementation).links;
        return make_forwarding_channel(*src_user, *dst_user, links, channels);
    }
    const auto src_node = std::get_if<node_endpoint>(&src);
    const auto dst_node = std::get_if<node_endpoint>(&dst);
    if (!src_node && !dst_node) {
        throw std::invalid_argument{"at least one end must be a node"};
    }
    const auto src_port_type = src_node
        ? validate(*src_node, node, io_type::out)
        : port_type::unknown;
    const auto dst_port_type = dst_node
        ? validate(*dst_node, node, io_type::in)
        : port_type::unknown;
    const auto src_dset = get_interface_ports(src_node);
    const auto dst_dset = get_interface_ports(dst_node);
    if (src_dset && dst_dset) {
        // TODO: make a forwarding channel for this case
        std::ostringstream os;
        os << "link between enclosing node endpoints not supported";
        throw std::invalid_argument{os.str()};
    }
    if (src_file) {
        return file_channel{src_file->path, io_type::in};
    }
    if (dst_file) {
        return file_channel{dst_file->path, io_type::out};
    }
    if (src_user || dst_user) {
        return pipe_channel{};
    }
    if (src_node && dst_node) {
        if (src_port_type != dst_port_type) {
            std::ostringstream os;
            os << "link between different port types not supported";
            os << ": src-type=" << int(src_port_type);
            os << ", dst-type=" << int(dst_port_type);
            throw std::invalid_argument{os.str()};
        }
        if (src_port_type == port_type::signal) {
            return make_signal_channel(*src_node, *dst_node);
        }
    }
    if (src_dset) {
        return make_reference_channel(*src_dset, name,
                                      parent_links, parent_channels);
    }
    if (dst_dset) {
        return make_reference_channel(*dst_dset, name,
                                      parent_links, parent_channels);
    }
    return pipe_channel{};
}

auto make_channel(const unidirectional_link& conn,
                  const node_name& name,
                  const node& node,
                  const std::span<channel>& channels,
                  const std::span<const link>& parent_links,
                  const std::span<channel>& parent_channels)
    -> channel
{
    try {
        return make_channel(conn.src, conn.dst, name, node, channels,
                            parent_links, parent_channels);
    }
    catch (const std::invalid_argument& ex) {
        throw invalid_link{conn, ex.what()};
    }
}

}

auto make_channel(const link& for_link,
                  const node_name& name,
                  const node& node,
                  const std::span<channel>& channels,
                  const std::span<const link>& parent_links,
                  const std::span<channel>& parent_channels)
    -> channel
{
    if (size(parent_links) != size(parent_channels)) {
        std::ostringstream os;
        os << "size of parent links (";
        os << size(parent_links);
        os << "), not equal size of parent channels (";
        os << size(parent_channels);
        os << ")";
        throw std::logic_error{os.str()};
    }
    if (const auto p = std::get_if<unidirectional_link>(&for_link)) {
        return make_channel(*p, name, node, channels,
                            parent_links, parent_channels);
    }
    throw invalid_link{for_link, "only unidirectional_link supported"};
}

auto operator<<(std::ostream& os, const reference_channel& value)
    -> std::ostream&
{
    os << "reference_channel{";
    os << value.other;
    if (value.other) {
        os << ", " << *value.other;
    }
    os << "}";
    return os;
}

}
