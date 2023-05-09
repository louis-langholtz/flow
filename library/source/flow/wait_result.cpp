#include "flow/wait_result.hpp"

namespace flow {

auto operator<<(std::ostream& os, const wait_result::no_kids_t&)
    -> std::ostream&
{
    os << "no child processes to wait for";
    return os;
}

auto operator<<(std::ostream& os, const wait_result::info_t& value)
    -> std::ostream&
{
    os << value.id << ", " << value.status;
    return os;
}

auto operator<<(std::ostream& os, const wait_result& value) -> std::ostream&
{
    if (value.holds_no_kids()) {
        os << value.no_kids();
    }
    else if (value.holds_error()) {
        os << value.error();
    }
    else if (value.holds_info()) {
        os << value.info();
    }
    return os;
}

}
