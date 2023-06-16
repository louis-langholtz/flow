#ifndef invalid_link_hpp
#define invalid_link_hpp

#include <stdexcept> // for std::invalid_argument

#include "flow/link.hpp"

namespace flow {

struct invalid_link: std::invalid_argument
{
    invalid_link(link l, const std::string& what_arg):
        std::invalid_argument(what_arg), value(std::move(l))
    {}
    invalid_link(link l, const char* what_arg):
        std::invalid_argument(what_arg), value(std::move(l))
    {}

    link value;
};

}

#endif /* invalid_link_hpp */
