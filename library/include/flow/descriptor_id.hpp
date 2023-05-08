#ifndef descriptor_id_hpp
#define descriptor_id_hpp

#include <ostream>
#include <type_traits> // for std::underlying_type_t

namespace flow {

enum class descriptor_id: int;

constexpr auto invalid_descriptor_id =
    descriptor_id{static_cast<std::underlying_type_t<descriptor_id>>(-1)};

auto operator<<(std::ostream& os, descriptor_id value) -> std::ostream&;

}

#endif /* descriptor_id_hpp */
