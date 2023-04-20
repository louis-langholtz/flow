#ifndef process_id_hpp
#define process_id_hpp

#include <cstdint> // for std::int32_t
#include <ostream>
#include <type_traits> // for std::underlying_type_t

namespace flow {

enum class process_id: std::int32_t;

constexpr auto invalid_process_id = process_id{static_cast<std::underlying_type_t<process_id>>(-1)};

std::ostream& operator<<(std::ostream& os, process_id value);

}

#endif /* process_id_hpp */
