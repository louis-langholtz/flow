#include "indenting_ostreambuf.hpp"

namespace flow::detail {

indenting_ostreambuf::indenting_ostreambuf(std::streambuf* dest,
                                           const options& opts)
    : myDest(dest)
    , myIsAtStartOfLine(opts.at_line_start)
    , myIndent(opts.indent, ' ')
{
}

indenting_ostreambuf::indenting_ostreambuf(std::ostream& dest,
                                           const options& opts)
    : myDest(dest.rdbuf())
    , myIsAtStartOfLine(opts.at_line_start)
    , myIndent(opts.indent, ' ')
    , myOwner(&dest)
{
    myOwner->rdbuf(this);
}

indenting_ostreambuf::~indenting_ostreambuf()
{
    if (myOwner) {
        myOwner->rdbuf(myDest);
    }
}

auto indenting_ostreambuf::overflow(int ch) -> int
{
    if (myIsAtStartOfLine && (ch != '\n')) {
        myDest->sputn(data(myIndent),
                      static_cast<std::streamsize>(size(myIndent)));
    }
    myIsAtStartOfLine = ch == '\n';
    return myDest->sputc(traits_type::to_char_type(ch));
}

}
