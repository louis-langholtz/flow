#include "flow/connection.hpp"

namespace flow {

std::ostream& operator<<(std::ostream& os, const file_connection& value)
{
    os << "file_connection{";
    os << ".file=" << value.file;
    os << ",.direction=" << value.direction;
    os << ",.process=" << value.process;
    os << "}";
    return os;
}

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
