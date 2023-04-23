#ifndef io_type_hpp
#define io_type_hpp

#include <ostream>

namespace flow {

enum class io_type: int { in = 0, out = 1};

std::ostream& operator<<(std::ostream& os, io_type value);

}

#endif /* io_type_hpp */
