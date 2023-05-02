#ifndef indenting_ostreambuf_hpp
#define indenting_ostreambuf_hpp

#include <ostream>
#include <streambuf>
#include <string>

namespace flow::detail {

struct indenting_ostreambuf_options {
    static constexpr auto default_indent = 4;
    static constexpr auto default_at_line_start = true;

    int indent{default_indent};
    bool at_line_start{default_at_line_start};
};

/// @brief Indenting streambuf.
/// @note Code derives from StackOverflow answer by "James Kanze" Mar 7, 2012.
/// @see https://stackoverflow.com/a/9600752/7410358
struct indenting_ostreambuf : public std::streambuf
{
    using options = indenting_ostreambuf_options;
    explicit indenting_ostreambuf(std::streambuf* dest,
                                  const options& opts = options());
    explicit indenting_ostreambuf(std::ostream& dest,
                                  const options& opts = options());
    ~indenting_ostreambuf() override;

private:
    std::streambuf* myDest{};
    bool myIsAtStartOfLine{true};
    std::string myIndent;
    std::ostream* myOwner{};

    auto overflow( int ch ) -> int override;
};

}

#endif /* indenting_ostreambuf_hpp */
