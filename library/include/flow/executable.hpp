#ifndef executable_hpp
#define executable_hpp

#include <concepts> // for std::regular.
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

namespace flow {

/// @brief Executable.
/// @note This is a <code>node</code> implementation type.
/// @see node.
struct executable
{
    /// @brief Path to the executable file for this node.
    std::filesystem::path file;

    /// @brief Arguments to pass to the executable.
    std::vector<std::string> arguments;

    /// @brief Working directory.
    /// @todo Consider what sense this member makes & removing it if
    ///   there isn't enough reason to keep it around.
    std::filesystem::path working_directory;
};

inline auto operator==(const executable& lhs,
                       const executable& rhs) noexcept -> bool
{
    return (lhs.file == rhs.file)
        && (lhs.arguments == rhs.arguments)
        && (lhs.working_directory == rhs.working_directory);
}

static_assert(std::regular<executable>);

auto operator<<(std::ostream& os, const executable& value) -> std::ostream&;

}

#endif /* executable_hpp */
