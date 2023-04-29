#ifndef fstream_hpp
#define fstream_hpp

#include <cassert>
#include <cstdint> // for std::uint32_t
#include <cstdio> // for FILE*
#include <cstring> // for std::strlen, std::memmove
#include <iostream>
#ifndef NDEBUG
#include <limits> // for std::numeric_limits
#endif
#include <memory> // for std::unique_ptr
#include <streambuf>
#include <string>
#include <filesystem>
#include <utility> // for std::move

#include <fcntl.h> // for O_CLOEXEC
#include <unistd.h>

namespace ext {

/// @brief File based I/O stream class for POSIX systems.
/// @note This supports C++23's <code>noreplace</code> and a new <code>tmpfile</code>
///   openmode. The latter results in the stream being opened for reading/writing to a temporary file that
///   either never shows up in the file system, or is deleted from it right after its creation. One has to use
///   the open modes however from this class; not from <code>std::ios_base</code> or any derived
///   classes of that other than this class.
/// @note Much of this class's implementation comes from LLVM's code for
///   <code>std::fstream</code> sans support for character encodings & locales. Any code herein
///   that may appear thrown together is likely mine or due to my effort to shoehorn LLVM's code into
///   this class without having to provide alternatives for additional standard library classes.
/// @see https://github.com/llvm/llvm-project/blob/main/libcxx/include/fstream.
struct fstream: public std::iostream {

    /// @brief Open mode.
    using openmode = std::uint32_t;

    static constexpr auto app       = openmode{0x001};
    static constexpr auto binary    = openmode{0x002};
    static constexpr auto in        = openmode{0x004};
    static constexpr auto out       = openmode{0x008};
    static constexpr auto trunc     = openmode{0x010};
    static constexpr auto ate       = openmode{0x020};

    /// @brief No-replace - provides exclusive file creation.
    /// @note Support C++23's <code>noreplace</code> open mode.
    /// @see https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2467r1.html
    static constexpr auto noreplace = openmode{0x040};

    /// @brief Temporary file.
    /// @note This is potentially a proposed new openmode by me!
    static constexpr auto tmpfile = openmode{0x080};

    /// @brief Close on exec.
    /// @note This is potentially a proposed new openmode by me!
    static constexpr auto cloexec = openmode{0x100};

    static constexpr auto to_fopen_mode(openmode value) -> const char*;

    struct filebuf: public std::streambuf {
        using int_type = typename traits_type::int_type;
        using pos_type = typename traits_type::pos_type;
        using off_type = typename traits_type::off_type;
        using state_type = typename traits_type::state_type;

        filebuf();
        filebuf(const filebuf& other) = delete;
        filebuf(filebuf&& other) noexcept;
        ~filebuf() override;
        auto operator=(const filebuf& other) -> filebuf& = delete;
        auto operator=(filebuf&& other) noexcept -> filebuf&;
        auto swap(filebuf& rhs) -> void;
        auto is_open() const noexcept -> bool;
        auto open(const char* path, openmode mode) -> filebuf*;
        auto open(const std::filesystem::path& path, openmode mode) -> filebuf*;
        auto open(const std::string& path, openmode mode) -> filebuf*;
        auto close() noexcept -> filebuf*;

    private:
        struct fcloser {
            auto operator()(FILE* p) const -> void {
                ::fclose(p); // NOLINT(cppcoreguidelines-owning-memory)
            }
        };

        static constexpr auto default_buffer_size = 4096u;
        static constexpr auto extbuf_min_size = 8u;

        auto internal_setbuf(char_type* s, std::streamsize n) -> basic_streambuf<char_type, traits_type>*;
        auto internal_sync() -> int;
        auto internal_overflow(int_type c = traits_type::eof()) -> int_type;

        /// @see https://en.cppreference.com/w/cpp/io/basic_streambuf/underflow.
        auto underflow() -> int_type override;

        /// @see https://en.cppreference.com/w/cpp/io/basic_streambuf/uflow.
        //int_type uflow() override;

        /// @see https://en.cppreference.com/w/cpp/io/basic_streambuf/pbackfail.
        auto pbackfail(int_type c = traits_type::eof()) -> int_type override;

        /// @see https://en.cppreference.com/w/cpp/io/basic_streambuf/overflow.
        auto overflow(int_type c = traits_type::eof()) -> int_type override;

        auto setbuf(char_type* s, std::streamsize n) -> basic_streambuf<char_type, traits_type>* override;

        /// @see https://en.cppreference.com/w/cpp/io/basic_streambuf/sputn.
        auto xsputn(const char* s, std::streamsize n) -> std::streamsize override;

        /// @see https://en.cppreference.com/w/cpp/io/basic_streambuf/pubseekoff.
        auto seekoff(off_type off, seekdir way,
                     std::ios_base::openmode wch = std::ios_base::in|std::ios_base::out) -> pos_type override;

        /// @see https://en.cppreference.com/w/cpp/io/basic_streambuf/pubseekpos.
        auto seekpos(pos_type sp,
                     std::ios_base::openmode wch = std::ios_base::in|std::ios_base::out) -> pos_type override;

        /// @see https://en.cppreference.com/w/cpp/io/basic_streambuf/pubsync.
        auto sync() -> int override;

        auto read_mode() -> bool;
        auto write_mode() -> void;

        char* extbuf_{};
        const char* extbufnext_{};
        const char* extbufend_{};
        char extbuf_min_[extbuf_min_size]{};
        size_t ebs_{};
        char_type* intbuf_{};
        size_t ibs_{};
        std::unique_ptr<FILE, fcloser> fp;
        state_type st_{};
        state_type st_last_{};
        openmode opened_mode{};
        openmode iomode{};
        bool owns_eb_{};
        bool owns_ib_{};
    };

    fstream();
    fstream(const fstream& other) = delete;
    fstream(fstream&& other) noexcept;

    auto operator=(const fstream& other) noexcept -> fstream& = delete;
    auto operator=(fstream&& other) noexcept -> fstream&;

    auto is_open() const -> bool;
    auto close() -> void;
    auto open(const char* path, openmode mode = in|out) -> void;
    auto open(const std::string& path, openmode mode = in|out) -> void;
    auto open(const std::filesystem::path& path, openmode mode = in|out) -> void;

private:
    filebuf fb;
};

constexpr auto fstream::to_fopen_mode(openmode value) -> const char*
{
    switch (value) {
    case out:
    case out|trunc:
        return "w";
    case out|noreplace:
    case out|trunc|noreplace:
        return "wx";
    case out|app:
    case app:
        return "a";
    case in:
        return "r";
    case in|out:
        return "r+";
    case in|out|trunc:
        return "w+";
    case in|out|trunc|noreplace:
        return "w+x";
    case in|out|app:
    case in|app:
        return "a+";
    case out|binary:
    case out|trunc|binary:
        return "wb";
    case out|app|binary:
    case app|binary:
        return "ab";
    case in|binary:
        return "rb";
    case in|out|binary:
        return "r+b";
    case in|out|trunc|binary:
        return "w+b";
    case in|out|trunc|binary|noreplace:
        return "w+xb";
    case in|out|app|binary:
    case in|app|binary:
        return "a+b";
    }
    return nullptr;
}

inline fstream::filebuf::filebuf()
{
    internal_setbuf(nullptr, default_buffer_size);
}

inline fstream::filebuf::filebuf(filebuf&& other) noexcept
    : std::streambuf(other)
{
    if (other.extbuf_ == std::data(other.extbuf_min_))
    {
        extbuf_ = std::data(extbuf_min_);
        extbufnext_ = extbuf_ + (other.extbufnext_ - other.extbuf_);
        extbufend_ = extbuf_ + (other.extbufend_ - other.extbuf_);
    }
    else
    {
        extbuf_ = other.extbuf_;
        extbufnext_ = other.extbufnext_;
        extbufend_ = other.extbufend_;
    }
    ebs_ = other.ebs_;
    intbuf_ = other.intbuf_;
    ibs_ = other.ibs_;
    fp = std::move(other.fp);
    st_ = other.st_;
    st_last_ = other.st_last_;
    opened_mode = other.opened_mode;
    iomode = other.iomode;
    owns_eb_ = other.owns_eb_;
    owns_ib_ = other.owns_ib_;
    if (other.pbase())
    {
        if (other.pbase() == other.intbuf_) {
            this->setp(intbuf_, intbuf_ + (other.epptr() - other.pbase()));
        }
        else {
            this->setp((char_type*)extbuf_,
                       (char_type*)extbuf_ + (other.epptr() - other.pbase()));
        }
        const auto diff = other.pptr() - other.pbase();
        assert(std::abs(diff) < std::numeric_limits<int>::max());
        this->pbump(static_cast<int>(diff));
    }
    else if (other.eback())
    {
        if (other.eback() == other.intbuf_) {
            this->setg(intbuf_, intbuf_ + (other.gptr() - other.eback()),
                       intbuf_ + (other.egptr() - other.eback()));
        }
        else {
            this->setg((char_type*)extbuf_,
                       (char_type*)extbuf_ + (other.gptr() - other.eback()),
                       (char_type*)extbuf_ + (other.egptr() - other.eback()));
        }
    }
    other.extbuf_ = nullptr;
    other.extbufnext_ = nullptr;
    other.extbufend_ = nullptr;
    other.ebs_ = 0;
    other.intbuf_ = nullptr;
    other.ibs_ = 0;
    other.fp = nullptr;
    other.st_ = state_type();
    other.st_last_ = state_type();
    other.opened_mode = 0;
    other.iomode = 0;
    other.owns_eb_ = false;
    other.owns_ib_ = false;
    other.setg(nullptr, nullptr, nullptr);
    other.setp(nullptr, nullptr);
}

inline fstream::filebuf::~filebuf()
{
    close();
    if (owns_eb_) {
        delete [] extbuf_;
    }
    if (owns_ib_) {
        delete [] intbuf_;
    }
}

inline auto fstream::filebuf::operator=(filebuf&& other) noexcept -> filebuf&
{
    close();
    swap(other);
    return *this;
}

inline auto fstream::filebuf::swap(filebuf& rhs) -> void
{
    basic_streambuf<char_type, traits_type>::swap(rhs);
    if ((extbuf_ != std::data(extbuf_min_)) && (rhs.extbuf_ != std::data(rhs.extbuf_min_)))
    {
        // Neither *this nor rhs uses the small buffer, so we can simply swap the pointers.
        std::swap(extbuf_, rhs.extbuf_);
        std::swap(extbufnext_, rhs.extbufnext_);
        std::swap(extbufend_, rhs.extbufend_);
    }
    else
    {
        const auto ln = extbufnext_     ? extbufnext_ - extbuf_         : 0;
        const auto le = extbufend_      ? extbufend_ - extbuf_          : 0;
        const auto rn = rhs.extbufnext_ ? rhs.extbufnext_ - rhs.extbuf_ : 0;
        const auto re = rhs.extbufend_  ? rhs.extbufend_ - rhs.extbuf_  : 0;
        if ((extbuf_ == std::data(extbuf_min_)) && (rhs.extbuf_ != std::data(rhs.extbuf_min_)))
        {
            // *this uses the small buffer, but rhs doesn't.
            extbuf_ = rhs.extbuf_;
            rhs.extbuf_ = std::data(rhs.extbuf_min_);
            std::memmove(std::data(rhs.extbuf_min_), std::data(extbuf_min_), sizeof(extbuf_min_));
        }
        else if (extbuf_ != std::data(extbuf_min_) && rhs.extbuf_ == std::data(rhs.extbuf_min_))
        {
            // *this doesn't use the small buffer, but rhs does.
            rhs.extbuf_ = extbuf_;
            extbuf_ = std::data(extbuf_min_);
            std::memmove(std::data(extbuf_min_), std::data(rhs.extbuf_min_), sizeof(extbuf_min_));
        }
        else
        {
            // Both *this and rhs use the small buffer.
            char tmp[sizeof(extbuf_min_)];
            std::memmove(std::data(tmp), std::data(extbuf_min_), sizeof(extbuf_min_));
            std::memmove(std::data(extbuf_min_), std::data(rhs.extbuf_min_), sizeof(extbuf_min_));
            std::memmove(std::data(rhs.extbuf_min_), std::data(tmp), sizeof(extbuf_min_));
        }
        extbufnext_ = extbuf_ + rn;
        extbufend_ = extbuf_ + re;
        rhs.extbufnext_ = rhs.extbuf_ + ln;
        rhs.extbufend_ = rhs.extbuf_ + le;
    }
    std::swap(ebs_, rhs.ebs_);
    std::swap(intbuf_, rhs.intbuf_);
    std::swap(ibs_, rhs.ibs_);
    std::swap(fp, rhs.fp);
    std::swap(st_, rhs.st_);
    std::swap(st_last_, rhs.st_last_);
    std::swap(opened_mode, rhs.opened_mode);
    std::swap(iomode, rhs.iomode);
    std::swap(owns_eb_, rhs.owns_eb_);
    std::swap(owns_ib_, rhs.owns_ib_);
    if (this->eback() == (char_type*)rhs.extbuf_min_)
    {
        const auto n = this->gptr() - this->eback();
        const auto e = this->egptr() - this->eback();
        this->setg((char_type*)extbuf_min_,
                   (char_type*)extbuf_min_ + n,
                   (char_type*)extbuf_min_ + e);
    }
    else if (this->pbase() == (char_type*)rhs.extbuf_min_)
    {
        const auto n = this->pptr() - this->pbase();
        const auto e = this->epptr() - this->pbase();
        this->setp((char_type*)extbuf_min_,
                   (char_type*)extbuf_min_ + e);
        assert(n < static_cast<std::ptrdiff_t>(std::numeric_limits<int>::max()));
        this->pbump(static_cast<int>(n));
    }
    if (rhs.eback() == (char_type*)extbuf_min_)
    {
        const auto n = rhs.gptr() - rhs.eback();
        const auto e = rhs.egptr() - rhs.eback();
        rhs.setg((char_type*)rhs.extbuf_min_,
                   (char_type*)rhs.extbuf_min_ + n,
                   (char_type*)rhs.extbuf_min_ + e);
    }
    else if (rhs.pbase() == (char_type*)extbuf_min_)
    {
        const auto n = rhs.pptr() - rhs.pbase();
        const auto e = rhs.epptr() - rhs.pbase();
        rhs.setp((char_type*)rhs.extbuf_min_,
                   (char_type*)rhs.extbuf_min_ + e);
        assert(n < static_cast<std::ptrdiff_t>(std::numeric_limits<int>::max()));
        rhs.pbump(static_cast<int>(n));
    }
}

inline void
swap(fstream::filebuf& x, fstream::filebuf& y)
{
    x.swap(y);
}

inline auto fstream::filebuf::is_open() const noexcept -> bool
{
    return fp != nullptr;
}

inline auto fstream::filebuf::open(const char* path, openmode mode) -> filebuf*
{
    if (fp) {
        return nullptr;
    }
    const auto mode_cstr = to_fopen_mode(mode & ~(tmpfile|cloexec));
    if (!mode_cstr) {
        return nullptr;
    }
    auto new_fp = std::unique_ptr<FILE, fcloser>{};
    if (mode & tmpfile) {
#if defined(O_TMPFILE)
        // TODO: translate mode to Linux/POSIX open(2) flags plus O_TMPFILE!
        auto oflags = 0;
        oflags |= O_TMPFILE;
        const auto fd = ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
                               path, oflags, 0666);
#else
        static constexpr auto buffer_size = std::size_t{1024u};
        static constexpr char six_x[] = "XXXXXX";
        char buffer[buffer_size];
        const auto len = std::strlen(path);
        const auto last_char = (len > 0u)? path[len - 1u]: '\0';
        const auto separator = (last_char != '/')? "/": "";
        const auto separator_len = std::strlen(separator);
        const auto total_len = len + separator_len + std::size(six_x);
        if (total_len >= buffer_size) {
            return nullptr;
        }
        // Replace the following with the more readable std::format_to_n when available
        std::copy_n(std::data(six_x), std::size(six_x),
                    std::copy_n(separator, separator_len,
                                std::copy_n(path, len, std::data(buffer))));
        const auto fd = ::mkstemp(std::data(buffer));
#endif
        if (fd == -1) {
            return nullptr;
        }
        new_fp = std::unique_ptr<FILE, fcloser>{::fdopen(fd, mode_cstr)};
        if (!new_fp) {
            ::close(fd);
            return nullptr;
        }
    }
    else {
        new_fp = std::unique_ptr<FILE, fcloser>{::fopen(path, mode_cstr)};
    }
    if (!new_fp) {
        return nullptr;
    }
    if (mode & cloexec) {
        ::fcntl( // NOLINT(cppcoreguidelines-pro-type-vararg)
                fileno(new_fp.get()), F_SETFD, FD_CLOEXEC);
    }
    if ((mode & ate) && (::fseek(new_fp.get(), 0, SEEK_END) != 0)) {
        return nullptr;
    }
    fp = std::move(new_fp);
    opened_mode = mode;
    return this;
}

inline auto fstream::filebuf::open(const std::filesystem::path& path, openmode mode) -> filebuf*
{
    return open(path.c_str(), mode);
}

inline auto fstream::filebuf::open(const std::string& path, openmode mode) -> filebuf*
{
    return open(path.c_str(), mode);
}

inline auto fstream::filebuf::close() noexcept -> filebuf*
{
    if (!fp) {
        return nullptr;
    }
    const auto sync_result = internal_sync();
    const auto close_result = ::fclose(fp.get());
    if (sync_result || (close_result == EOF)) {
        return nullptr;
    }
    (void) fp.release();
    opened_mode = 0;
    internal_setbuf(nullptr, 0);
    return this;
}

inline auto fstream::filebuf::underflow() -> int_type
{
    if (!fp) {
        return traits_type::eof();
    }
    const auto mode_changed = read_mode();
    char_type char_buf{};
    if (!this->gptr()) {
        this->setg(&char_buf, &char_buf+1, &char_buf+1);
    }
    const size_t unget_sz = mode_changed ? 0 : std::min<size_t>((this->egptr() - this->eback()) / 2, 4);
    int_type c = traits_type::eof();
    if (this->gptr() == this->egptr())
    {
        std::memmove(this->eback(), this->egptr() - unget_sz, unget_sz * sizeof(char_type));
        size_t nmemb = static_cast<size_t>(this->egptr() - this->eback() - unget_sz);
        nmemb = ::fread(this->eback() + unget_sz, 1, nmemb, fp.get());
        if (nmemb != 0)
        {
            this->setg(this->eback(),
                       this->eback() + unget_sz,
                       this->eback() + unget_sz + nmemb);
            c = traits_type::to_int_type(*this->gptr());
        }
    }
    else {
        c = traits_type::to_int_type(*this->gptr());
    }
    if (this->eback() == &char_buf) {
        this->setg(nullptr, nullptr, nullptr);
    }
    return c;
}

inline auto fstream::filebuf::pbackfail(int_type c) -> int_type
{
    if (fp && this->eback() < this->gptr())
    {
        if (traits_type::eq_int_type(c, traits_type::eof()))
        {
            this->gbump(-1);
            return traits_type::not_eof(c);
        }
        if ((opened_mode & out) ||
            traits_type::eq(traits_type::to_char_type(c), this->gptr()[-1]))
        {
            this->gbump(-1);
            *this->gptr() = traits_type::to_char_type(c);
            return c;
        }
    }
    return traits_type::eof();
}

inline auto fstream::filebuf::internal_overflow(int_type c) -> int_type
{
    if (!fp) {
        return traits_type::eof();
    }
    write_mode();
    char_type char_buf{};
    char_type* pb_save = this->pbase();
    char_type* epb_save = this->epptr();
    if (!traits_type::eq_int_type(c, traits_type::eof())) {
        if (!pptr()) {
            setp(&char_buf, &char_buf+1);
        }
        *pptr() = traits_type::to_char_type(c);
        pbump(1);
    }
    if (pptr() != pbase()) {
        const auto nmemb = static_cast<size_t>(this->pptr() - this->pbase());
        if (std::fwrite(this->pbase(), sizeof(char_type), nmemb, fp.get()) != nmemb) {
            return traits_type::eof();
        }
        setp(pb_save, epb_save);
    }
    return traits_type::not_eof(c);
}

inline auto fstream::filebuf::overflow(int_type c) -> int_type
{
    return internal_overflow(c);
}

inline auto fstream::filebuf::internal_setbuf(char_type* s, std::streamsize n)
    -> basic_streambuf<char_type, traits_type>*
{
    assert(n < std::numeric_limits<int>::max());
    setg(nullptr, nullptr, nullptr);
    setp(nullptr, nullptr);
    if (owns_eb_) {
        delete [] extbuf_;
    }
    if (owns_ib_) {
        delete [] intbuf_;
    }
    ebs_ = n;
    if (ebs_ > sizeof(extbuf_min_))
    {
        if (s) {
            extbuf_ = (char*)s;
            owns_eb_ = false;
        }
        else {
            extbuf_ = new char[ebs_]; // NOLINT(cppcoreguidelines-owning-memory)
            owns_eb_ = true;
        }
    }
    else {
        extbuf_ = std::data(extbuf_min_);
        ebs_ = sizeof(extbuf_min_);
        owns_eb_ = false;
    }
    ibs_ = 0;
    intbuf_ = nullptr;
    owns_ib_ = false;
    return this;
}

inline auto fstream::filebuf::setbuf(char_type* s, std::streamsize n)
    -> basic_streambuf<char_type, traits_type>*
{
    return internal_setbuf(s, n);
}

inline auto fstream::filebuf::xsputn(const char* s, std::streamsize n)
    -> std::streamsize
{
    if (!(iomode & out))
    {
        setg(nullptr, nullptr, nullptr);
        setp(nullptr, nullptr);
        iomode = out;
    }
    assert(n >= 0);
    const auto rv = std::fwrite(s, sizeof(char_type),
                                static_cast<std::size_t>(n), fp.get());
    return static_cast<std::streamsize>(rv);
}

inline auto fstream::filebuf::internal_sync() -> int
{
    if (!fp) {
        return 0;
    }
    if (iomode & out)
    {
        if (pptr() != pbase()) {
            if (internal_overflow() == traits_type::eof()) {
                return -1;
            }
        }
        if (std::fflush(fp.get())) {
            return -1;
        }
    }
    else if (iomode & in)
    {
        const auto c = egptr() - gptr();
        if (::fseeko(fp.get(), -c, SEEK_CUR)) {
            return -1;
        }
        setg(nullptr, nullptr, nullptr);
        iomode = 0;
    }
    return 0;
}

inline auto fstream::filebuf::sync() -> int
{
    return internal_sync();
}

inline auto
fstream::filebuf::seekoff(off_type off, seekdir way, std::ios_base::openmode) -> pos_type
{
    if (!fp || sync()) {
        return static_cast<pos_type>(static_cast<off_type>(-1));
    }
    int whence{};
    switch (way)
    {
    case beg:
        whence = SEEK_SET;
        break;
    case cur:
        whence = SEEK_CUR;
        break;
    case end:
        whence = SEEK_END;
        break;
    default:
        return static_cast<pos_type>(static_cast<off_type>(-1));
    }
    if (::fseeko(fp.get(), off, whence)) {
        return static_cast<pos_type>(static_cast<off_type>(-1));
    }
    pos_type r = ftello(fp.get());
    r.state(st_);
    return r;
}

inline auto
fstream::filebuf::seekpos(pos_type sp, std::ios_base::openmode) -> pos_type
{
    if (!fp || sync()) {
        return static_cast<pos_type>(static_cast<off_type>(-1));
    }
    if (::fseeko(fp.get(), sp, SEEK_SET)) {
        return static_cast<pos_type>(static_cast<off_type>(-1));
    }
    st_ = sp.state();
    return sp;
}

inline auto fstream::filebuf::read_mode() -> bool
{
    if (!(iomode & in))
    {
        setp(nullptr, nullptr);
        setg((char_type*)extbuf_,
             (char_type*)extbuf_ + ebs_,
             (char_type*)extbuf_ + ebs_);
        iomode = in;
        return true;
    }
    return false;
}

inline auto fstream::filebuf::write_mode() -> void
{
    if (!(iomode & out))
    {
        setg(nullptr, nullptr, nullptr);
        if (ebs_ > sizeof(extbuf_min_)) {
            setp((char_type*)extbuf_,
                 (char_type*)extbuf_ + (ebs_ - 1));
        }
        else {
            setp(nullptr, nullptr);
        }
        iomode = out;
    }
}

inline fstream::fstream():
    std::iostream{&fb}
{
    // Intentionally empty.
}

inline fstream::fstream(fstream&& other) noexcept:
    std::iostream{std::move(other)},
    fb{std::move(other.fb)}
{
    set_rdbuf(&fb);
}

inline auto fstream::operator=(fstream&& other) noexcept -> fstream&
{
    std::iostream::operator=(std::move(other));
    fb = std::move(other.fb); // NOLINT(bugprone-use-after-move)
    return *this;
}

inline auto fstream::is_open() const -> bool
{
    return fb.is_open();
}

inline auto fstream::close() -> void
{
    if (!fb.close()) {
        setstate(ios_base::failbit);
    }
}

inline auto fstream::open(const char* path, openmode mode) -> void
{
    if (fb.open(path, mode)) {
        clear();
    }
    else {
        setstate(ios_base::failbit);
    }
}

inline auto fstream::open(const std::string& path, openmode mode) -> void
{
    open(path.c_str(), mode);
}

inline auto fstream::open(const std::filesystem::path& path,
                          openmode mode) -> void
{
    open(path.c_str(), mode);
}

}

#endif /* fstream_hpp */