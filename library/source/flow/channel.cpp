#include <cerrno> // for errno
#include <cstring> // for std::streror
#include <iostream>
#include <optional>
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
    const auto at = [](const descriptor_map& descriptors,
                       const reference_descriptor& key,
                       const system_name& name) -> const descriptor_info&
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

auto make_channel(const unidirectional_connection& conn,
                  const system_name& name,
                  const system& system,
                  const std::span<const connection>& parent_connections,
                  const std::span<channel>& parent_channels)
    -> channel
{
    static constexpr auto no_file_file_error =
        "can't connect file to file";
    static constexpr auto no_user_user_error =
        "can't connect user to user";
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
        throw invalid_connection{no_file_file_error};
    }
    const auto src_user = std::get_if<user_endpoint>(&conn.src);
    const auto dst_user = std::get_if<user_endpoint>(&conn.dst);
    if (src_user && dst_user) {
        throw invalid_connection{no_user_user_error};
    }
    const auto src_system = std::get_if<system_endpoint>(&conn.src);
    const auto dst_system = std::get_if<system_endpoint>(&conn.dst);
    if (!src_system && !dst_system) {
        throw invalid_connection{no_system_end_error};
    }
    auto enclosure_descriptors =
        std::array<std::optional<std::set<reference_descriptor>>, 2u>{};
    if (src_system) {
        validate(*src_system, system, io_type::out);
        if (src_system->address == system_name{}) {
            enclosure_descriptors[0] = src_system->descriptors;
        }
    }
    if (dst_system) {
        validate(*dst_system, system, io_type::in);
        if (dst_system->address == system_name{}) {
            enclosure_descriptors[1] = dst_system->descriptors;
        }
    }
    if (src_file) {
        return {file_channel{src_file->path, io_type::in}};
    }
    if (dst_file) {
        return {file_channel{dst_file->path, io_type::out}};
    }
    if (src_user || dst_user) {
        return {pipe_channel{}};
    }
    for (auto&& descriptor_set: enclosure_descriptors) {
        if (descriptor_set) {
            const auto look_for = system_endpoint{name, *descriptor_set};
            if (const auto found = find_index(parent_connections, look_for)) {
                return {reference_channel{&parent_channels[*found]}};
            }
            std::ostringstream os;
            os << "can't find parent connection with ";
            os << look_for;
            os << " endpoint for ";
            os << conn;
            throw invalid_connection{os.str()};
        }
    }
    return {pipe_channel{}};
}

}

auto make_channel(const connection& conn,
                  const system_name& name,
                  const system& system,
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
        return make_channel(*p, name, system,
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
