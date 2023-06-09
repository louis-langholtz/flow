#ifndef dstream_hpp
#define dstream_hpp

#include <array>
#include <cstddef> // for std::ptrdiff_t
#include <iostream>
#include <string> // fpr std::char_traits

#include "flow/reference_descriptor.hpp"

namespace flow {

/// @brief Descriptor stream buffer.
/// @note Some of this class's implementation derives from Nicolai M.
///   Josuttis handling for <code>boost::fdinbuf</code> and
///   <code>boost::fdoutbuf</code>.
/// @see http://www.josuttis.com/cppcode/fdstream.hpp.
struct descriptor_streambuf: public std::streambuf {
public:
    using char_type = char;
    using traits_type = std::char_traits<char_type>;
    using int_type = typename traits_type::int_type;
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;
    using state_type = typename traits_type::state_type;

    descriptor_streambuf(reference_descriptor d): id(d)
    {
        // Intentionally empty.
    }

protected:
    auto underflow() -> int_type override;
    auto overflow(int_type c = traits_type::eof()) -> int_type override;
    auto xsputn(const char_type* s, std::streamsize n)
        -> std::streamsize override;
    // TODO: override sync()

private:
    static constexpr auto buffer_size = 1024u;
    static constexpr auto putback_size = std::ptrdiff_t{8};
    reference_descriptor id{descriptors::invalid_id};
    std::array<char, buffer_size> buffer{};
};

struct descriptor_istream: public std::istream {
    descriptor_istream(reference_descriptor fd)
        : std::istream(nullptr), streambuf(fd)
    {
        rdbuf(&streambuf);
    }
private:
    descriptor_streambuf streambuf;
};

struct descriptor_ostream: public std::ostream {
    descriptor_ostream(reference_descriptor fd)
        : std::ostream(nullptr), streambuf(fd)
    {
        rdbuf(&streambuf);
    }
private:
    descriptor_streambuf streambuf;
};

struct descriptor_iostream: public std::iostream {
    descriptor_iostream(reference_descriptor fd)
        : std::iostream(nullptr), streambuf(fd)
    {
        rdbuf(&streambuf);
    }
private:
    descriptor_streambuf streambuf;
};

}

#endif /* dstream_hpp */
