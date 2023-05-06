#include <cerrno> // for errno
#include <cstring> // for std::streror
#include <iostream>
#include <sstream> // for std::ostringstream

#include "flow/channel.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace flow {

namespace {

auto validate(const system_endpoint& end,
              const system& system,
              io_type expected_io) -> void
{
    if (end.address == system_name{}) {
        const auto& d_info = system.descriptors.at(end.descriptor);
        if (d_info.direction != reverse(expected_io)) {
            throw std::invalid_argument{"bad custom system endpoint io"};
        }
        return;
    }
    if (const auto p = std::get_if<system::custom>(&(system.info))) {
        const auto& subsys = p->subsystems.at(end.address);
        const auto& d_info = subsys.descriptors.at(end.descriptor);
        if (d_info.direction != expected_io) {
            throw std::invalid_argument{"bad subsys endpoint io"};
        }
        return;
    }

}

auto make_not_closed_msg(const system_name& name,
                         const connection& conn,
                         const endpoint& look_for,
                         const std::span<const connection>& connections)
    -> std::string
{
    std::ostringstream os;
    static const auto not_closed_error = "system must be closed";
    os << not_closed_error;
    os << ":\n  for system " << name;
    os << "\n  for connection " << conn;
    os << "\n  looking-for " << look_for;
    if (connections.empty()) {
        os << "\n  parent-connections empty";
    }
    else {
        os << "\n  parent-connections:";
        for (auto&& c: connections) {
            os << "\n    " << c;
        }
    }
    return os.str();
}

auto make_channel(const system_name& name,
                  const system& system,
                  const unidirectional_connection& conn,
                  const std::span<const connection>& connections,
                  const std::span<channel>& channels)
    -> channel
{
    static constexpr auto unequal_sizes_error =
        "size of parent connections not equal size of parent channels";
    static constexpr auto no_file_file_error =
        "can't connect file to file";
    static constexpr auto no_user_user_error =
        "can't connect user to user";
    static constexpr auto same_endpoints_error =
        "connection must have different endpoints";
    static constexpr auto no_system_end_error =
        "at least one end must be a system";

    if (conn.src == conn.dst) {
        throw std::invalid_argument{same_endpoints_error};
    }
    if (std::size(connections) != std::size(channels)) {
        throw std::invalid_argument{unequal_sizes_error};
    }

    auto enclosure_descriptors = std::array<descriptor_id, 2u>{
        invalid_descriptor_id, invalid_descriptor_id
    };
    const auto src_file = std::get_if<file_endpoint>(&conn.src);
    const auto dst_file = std::get_if<file_endpoint>(&conn.dst);
    if (src_file && dst_file) {
        throw std::invalid_argument{no_file_file_error};
    }
    const auto src_user = std::get_if<user_endpoint>(&conn.src);
    const auto dst_user = std::get_if<user_endpoint>(&conn.dst);
    if (src_user && dst_user) {
        throw std::invalid_argument{no_user_user_error};
    }
    const auto src_system = std::get_if<system_endpoint>(&conn.src);
    const auto dst_system = std::get_if<system_endpoint>(&conn.dst);
    if (!src_system && !dst_system) {
        throw std::invalid_argument{no_system_end_error};
    }
    if (src_system) {
        validate(*src_system, system, io_type::out);
        if (src_system->address == system_name{}) {
            enclosure_descriptors[0] = src_system->descriptor;
        }
    }
    if (dst_system) {
        validate(*dst_system, system, io_type::in);
        if (dst_system->address == system_name{}) {
            enclosure_descriptors[1] = dst_system->descriptor;
        }
    }
    if (src_file) {
        return {file_channel{io_type::in}};
    }
    if (dst_file) {
        return {file_channel{io_type::out}};
    }
    if (src_user || dst_user) {
        return {pipe_channel{}};
    }
    for (auto&& descriptor_id: enclosure_descriptors) {
        if (descriptor_id != invalid_descriptor_id) {
            const auto look_for = system_endpoint{name, descriptor_id};
            if (const auto found = find_index(connections, look_for)) {
                return {reference_channel{&channels[*found]}};
            }
            throw std::invalid_argument{make_not_closed_msg(name, conn,
                                                            look_for,
                                                            connections)};
        }
    }
    return {pipe_channel{}};
}

}

auto make_channel(const system_name& name,
                  const system& system,
                  const connection& conn,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> channel
{
    if (const auto p = std::get_if<unidirectional_connection>(&conn)) {
        return make_channel(name, system, *p,
                            parent_connections, parent_channels);
    }
    throw std::invalid_argument{"only unidirectional_connection supported"};
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
