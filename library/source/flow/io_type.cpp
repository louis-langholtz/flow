#include "flow/io_type.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, io_type value)
{
    switch (value) {
    case flow::io_type::in:
        os << "in";
        break;
    case flow::io_type::out:
        os << "out";
        break;
    }
    return os;
}

}
