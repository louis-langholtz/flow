#include <iomanip> // for std::quoted

#include "flow/endpoint.hpp"

namespace flow {

auto operator>>(std::istream& is, endpoint& value) -> std::istream&
{
    if (is.fail()) {
        return is;
    }
    {
        auto tmp = unset_endpoint{};
        is >> tmp;
        if (!is.fail()) {
            value = tmp;
            return is;
        }
    }
    is.clear();
    {
        auto tmp = user_endpoint{};
        is >> tmp;
        if (!is.fail()) {
            value = tmp;
            return is;
        }
    }
    is.clear();
    {
        auto tmp = node_endpoint{};
        is >> tmp;
        if (!is.fail()) {
            value = tmp;
            return is;
        }
    }
    is.clear();
    {
        auto tmp = file_endpoint{};
        is >> tmp;
        if (!is.fail()) {
            value = tmp;
            return is;
        }
    }
    return is;
}

}
