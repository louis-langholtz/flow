#ifndef descriptor_id_hpp
#define descriptor_id_hpp

#include <ostream>
#include <type_traits> // for std::underlying_type_t

namespace flow {

enum class descriptor_id: int;

auto operator<<(std::ostream& os, descriptor_id value) -> std::ostream&;

namespace descriptors {
constexpr auto invalid_id = descriptor_id{-1};
constexpr auto stdin_id = descriptor_id{0};
constexpr auto stdout_id = descriptor_id{1};
constexpr auto stderr_id = descriptor_id{2};
}

}

#endif /* descriptor_id_hpp */
