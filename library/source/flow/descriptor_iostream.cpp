#include <cstring> // for std::memmove
#include <unistd.h> // for ::close

#include "flow/descriptor_iostream.hpp"

namespace flow {

auto descriptor_streambuf::underflow() -> int_type
{
    if (gptr() < egptr()) {
        return traits_type::to_int_type(*gptr());
    }

    const auto nputback = std::min(gptr() - eback(), putback_size);
    std::memmove(buffer.data() + (putback_size - nputback),
                 gptr() - nputback, nputback);
    const auto nread = ::read(int(id), buffer.data() + putback_size, buffer.size());
    if (nread == -1) {
        return traits_type::eof();
    }
    setg(buffer.data() + (putback_size - nputback),
         buffer.data() + putback_size,
         buffer.data() + putback_size + nread);
    return traits_type::to_int_type(*gptr());
}

auto descriptor_streambuf::overflow(int_type c) -> int_type
{
    if (!traits_type::eq_int_type(c, traits_type::eof()))
    {
        char_type onebuf = traits_type::to_char_type(c);
        if (::write(int(id), &onebuf, sizeof(char_type)) != 1) {
            return traits_type::eof();
        }
    }
    return traits_type::not_eof(c);
}

auto descriptor_streambuf::xsputn(const char_type* s, std::streamsize n)
    -> std::streamsize
{
    return ::write(int(id), s, n);
}

}
