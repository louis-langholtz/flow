#ifndef dstream_hpp
#define dstream_hpp

#include <array>
#include <cstddef> // for std::ptrdiff_t
#include <iostream>
#include <string> // fpr std::char_traits

#include "flow/descriptor_id.hpp"

namespace flow {

/// @brief Descriptor stream buffer.
/// @note Some of this class's implementation derives from Nicolai M. Josuttis handling for
///   <code>boost::fdinbuf</code> and <code>boost::fdoutbuf</code>.
/// @see http://www.josuttis.com/cppcode/fdstream.hpp.
struct descriptor_streambuf: public std::streambuf {
public:
    using char_type = char;
    using traits_type = std::char_traits<char_type>;
    using int_type = typename traits_type::int_type;
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;
    using state_type = typename traits_type::state_type;

    descriptor_streambuf(descriptor_id d): id(d)
    {
        // Intentionally empty.
    }

protected:
    int_type underflow() override;
    int_type overflow(int_type c = traits_type::eof()) override;
    std::streamsize xsputn(const char_type* s, std::streamsize n) override;
    // TODO: override sync()

private:
    static constexpr auto buffer_size = 1024u;
    static constexpr auto putback_size = std::ptrdiff_t{8};
    descriptor_id id{invalid_descriptor_id};
    std::array<char, buffer_size> buffer{};
};

struct descriptor_istream: public std::istream {
    descriptor_istream(descriptor_id fd)
        : std::istream(nullptr), streambuf(fd)
    {
        rdbuf(&streambuf);
    }
private:
    descriptor_streambuf streambuf;
};

struct descriptor_ostream: public std::ostream {
    descriptor_ostream(descriptor_id fd)
        : std::ostream(nullptr), streambuf(fd)
    {
        rdbuf(&streambuf);
    }
private:
    descriptor_streambuf streambuf;
};

struct descriptor_iostream: public std::iostream {
    descriptor_iostream(descriptor_id fd)
        : std::iostream(nullptr), streambuf(fd)
    {
        rdbuf(&streambuf);
    }
private:
    descriptor_streambuf streambuf;
};

}

#endif /* dstream_hpp */
