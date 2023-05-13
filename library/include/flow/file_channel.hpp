#ifndef file_channel_hpp
#define file_channel_hpp

#include <filesystem>
#include <ostream>
#include <type_traits> // for std::is_default_constructible_v

#include "flow/io_type.hpp"

namespace flow {

/// @brief File channel.
/// @note This is intended to be a strong place-holder/tag type.
/// @note Instances of this type are made for connections with
///   <code>file_endpoint</code> ends.
/// @see file_endpoint.
struct file_channel
{
    std::filesystem::path path;
    io_type io{};
    auto operator==(const file_channel&) const -> bool = default;
};

static_assert(std::is_default_constructible_v<file_channel>);

auto operator<<(std::ostream& os, const file_channel& value) -> std::ostream&;

}

#endif /* file_channel_hpp */
