#ifndef executable_hpp
#define executable_hpp

#include <filesystem>
#include <ostream>
#include <string>
#include <type_traits> // for std::is_default_constructible_v
#include <vector>

namespace flow {

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

static_assert(std::is_default_constructible_v<executable>);
static_assert(std::is_copy_constructible_v<executable>);
static_assert(std::is_move_constructible_v<executable>);
static_assert(std::is_copy_assignable_v<executable>);
static_assert(std::is_move_assignable_v<executable>);

inline auto operator==(const executable& lhs,
                       const executable& rhs) noexcept -> bool
{
    return (lhs.file == rhs.file)
        && (lhs.arguments == rhs.arguments)
        && (lhs.working_directory == rhs.working_directory);
}

auto operator<<(std::ostream& os, const executable& value) -> std::ostream&;

}

#endif /* executable_hpp */
