#include <cerrno> // for errno
#include <cstring> // for std::streror
#include <iostream>
#include <optional>
#include <sstream> // for std::ostringstream

#include <fcntl.h> // for ::open

#include "flow/channel.hpp"
#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace flow {

namespace {

auto validate(const system_endpoint& end,
              const system& system,
              io_type expected_io) -> void
{
    const auto at = [](const port_map& descriptors,
                       const reference_descriptor& key,
                       const system_name& name) -> const port_info&
    {
        try {
            return descriptors.at(key);
        }
        catch (const std::out_of_range& ex) {
            std::ostringstream os;
            os << "can't find " << key << " in ";
            if (name == system_name{}) {
                os << "system's";
            }
            else {
                os << name << " subsystem's";
            }
            os << " descriptor mapping: ";
            os << ex.what();
            throw invalid_connection{os.str()};
        }
    };
    if (end.address == system_name{}) {
        for (auto&& d: end.descriptors) {
            const auto& d_info = at(system.descriptors, d, end.address);
            if (d_info.direction != reverse(expected_io)) {
                throw invalid_connection{"bad custom system endpoint io"};
            }
        }
        return;
    }
    if (const auto p = std::get_if<system::custom>(&(system.info))) {
        const auto found = p->subsystems.find(end.address);
        if (found == p->subsystems.end()) {
            std::ostringstream os;
            os << "endpoint address system ";
            os << end.address;
            os << " not found";
            throw invalid_connection{os.str()};
        }
        const auto& subsys = p->subsystems.at(end.address);
        for (auto&& d: end.descriptors) {
            const auto& d_info = at(subsys.descriptors, d, end.address);
            if (d_info.direction != expected_io) {
                throw invalid_connection{"bad subsys endpoint io"};
            }
        }
        return;
    }

}

auto make_channel(const file_endpoint& src, const file_endpoint& dst)
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
        throw invalid_connection{os.str()};
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
        throw invalid_connection{os.str()};
    }
    return {std::move(src_d), std::move(dst_d)};
}

auto make_channel(const pipe_channel& src, const pipe_channel& dst)
    -> forwarding_channel
{
    return {
        src.get(pipe_channel::io::read),
        dst.get(pipe_channel::io::write)
    };
}

auto make_channel(const std::set<reference_descriptor>& dset,
                  const system_name& name,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> reference_channel
{
    const auto look_for = system_endpoint{name, dset};
    const auto found = find_index(parent_connections, look_for);
    if (!found) {
        std::ostringstream os;
        os << "can't find parent connection with ";
        os << look_for;
        os << " endpoint for making reference channel";
        throw invalid_connection{os.str()};
    }
    return {&parent_channels[*found]};
}

auto make_channel(const user_endpoint& src, const user_endpoint& dst,
                  const std::span<const connection>& connections,
                  const std::span<channel>& channels)
    -> forwarding_channel
{
    const auto src_conn = find_index(connections, src);
    if (!src_conn) {
        std::ostringstream os;
        os << "can't find source connection with endpoint ";
        os << src;
        throw invalid_connection{os.str()};
    }
    const auto dst_conn = find_index(connections, dst);
    if (!dst_conn) {
        std::ostringstream os;
        os << "can't find destination connection with endpoint ";
        os << dst;
        throw invalid_connection{os.str()};
    }
    return make_channel(std::get<pipe_channel>(channels[*src_conn]),
                        std::get<pipe_channel>(channels[*dst_conn]));
}

auto make_channel(const unidirectional_connection& conn,
                  const system_name& name,
                  const system& system,
                  const std::span<channel>& channels,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> channel
{
    static constexpr auto same_endpoints_error =
        "connection must have different endpoints";
    static constexpr auto no_system_end_error =
        "at least one end must be a system";
    if (conn.src == conn.dst) {
        throw invalid_connection{same_endpoints_error};
    }
    const auto src_file = std::get_if<file_endpoint>(&conn.src);
    const auto dst_file = std::get_if<file_endpoint>(&conn.dst);
    if (src_file && dst_file) {
        return make_channel(*src_file, *dst_file);
    }
    const auto src_user = std::get_if<user_endpoint>(&conn.src);
    const auto dst_user = std::get_if<user_endpoint>(&conn.dst);
    if (src_user && dst_user) {
        return make_channel(*src_user, *dst_user,
                            std::get<system::custom>(system.info).connections,
                            channels);
    }
    const auto src_system = std::get_if<system_endpoint>(&conn.src);
    const auto dst_system = std::get_if<system_endpoint>(&conn.dst);
    if (!src_system && !dst_system) {
        throw invalid_connection{no_system_end_error};
    }
    auto src_dset = std::optional<std::set<reference_descriptor>>{};
    auto dst_dset = std::optional<std::set<reference_descriptor>>{};
    if (src_system) {
        validate(*src_system, system, io_type::out);
        if (src_system->address == system_name{}) {
            src_dset = src_system->descriptors;
        }
    }
    if (dst_system) {
        validate(*dst_system, system, io_type::in);
        if (dst_system->address == system_name{}) {
            dst_dset = dst_system->descriptors;
        }
    }
    if (src_dset && dst_dset) {
        // TODO: make a forwarding channel for this case
        std::ostringstream os;
        os << "connection between enclosing system endpoints not supported";
        throw invalid_connection{os.str()};
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
    if (src_dset) {
        return make_channel(*src_dset, name,
                            parent_connections, parent_channels);
    }
    if (dst_dset) {
        return make_channel(*dst_dset, name,
                            parent_connections, parent_channels);
    }
    return pipe_channel{};
}

}

auto make_channel(const connection& conn,
                  const system_name& name,
                  const system& system,
                  const std::span<channel>& channels,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> channel
{
    if (size(parent_connections) != size(parent_channels)) {
        std::ostringstream os;
        os << "size of parent connections (";
        os << size(parent_connections);
        os << "), not equal size of parent channels (";
        os << size(parent_channels);
        os << ")";
        throw std::logic_error{os.str()};
    }
    if (const auto p = std::get_if<unidirectional_connection>(&conn)) {
        return make_channel(*p, name, system, channels,
                            parent_connections, parent_channels);
    }
    throw invalid_connection{"only unidirectional_connection supported"};
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
