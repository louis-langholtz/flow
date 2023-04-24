#include "flow/connection.hpp"

namespace flow {

auto operator<<(std::ostream& os, const file_connection& value) -> std::ostream&
{
    os << "file_connection{";
    os << ".file=" << value.file;
    os << ",.direction=" << value.direction;
    os << ",.process=" << value.process;
    os << "}";
    return os;
}

auto operator<<(std::ostream& os, const pipe_connection& value) -> std::ostream&
{
    os << "pipe_connection{";
    os << ".in=" << value.in;
    os << ",";
    os << ".out=" << value.out;
    os << "}";
    return os;
}

}
