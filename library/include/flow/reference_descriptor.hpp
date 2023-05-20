#ifndef descriptor_id_hpp
#define descriptor_id_hpp

#include <ostream>
#include <type_traits> // for std::underlying_type_t

namespace flow {

enum class reference_descriptor: int;

auto operator<<(std::ostream& os, reference_descriptor value) -> std::ostream&;

namespace descriptors {
constexpr auto invalid_id = reference_descriptor{-1};
constexpr auto stdin_id = reference_descriptor{0};
constexpr auto stdout_id = reference_descriptor{1};
constexpr auto stderr_id = reference_descriptor{2};
}

}

#endif /* descriptor_id_hpp */
