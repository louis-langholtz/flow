#include "pipe_connection.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const pipe_connection& value)
{
    os << "pipe_connection{";
    os << ".in=" << value.in;
    os << ",";
    os << ".out=" << value.out;
    os << "}";
    return os;
}

}
