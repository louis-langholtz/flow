#include <fcntl.h> // for O_CLOEXEC
#include <unistd.h>

#include <ext/fstream.hpp>

namespace ext {

auto filebuf::unique(std::filesystem::path& path) -> filebuf*
{
    if (fp) {
        return nullptr;
    }
    constexpr auto mode = in|out|binary;
    const auto mode_cstr = to_fopen_mode(mode);
    if (!mode_cstr) {
        return nullptr;
    }
    static constexpr char six_x[] = "XXXXXX"; // NOLINT(modernize-avoid-c-arrays)
    const auto total_len = size(path.native()) + std::size(six_x);
    if (total_len >= L_tmpnam) {
        return nullptr;
    }
    auto buffer = std::array<char, L_tmpnam>{};
    const auto extension = path.extension();
    auto new_filename = path.stem();
    new_filename += std::data(six_x);
    new_filename += extension;
    auto new_path = path;
    new_path.replace_filename(new_filename);
    std::copy_n(data(new_path.native()), size(new_path.native()), data(buffer));
    const auto fd = ::mkstemps(data(buffer),
                               static_cast<int>(size(extension.native())));
    if (fd == -1) {
        return nullptr;
    }
    auto new_fp = std::unique_ptr<FILE, fcloser>{::fdopen(fd, mode_cstr)};
    if (!new_fp) {
        ::close(fd);
        return nullptr;
    }
    path = data(buffer);
    fp = std::move(new_fp);
    opened_mode = mode;
    return this;
}

auto filebuf::open(const char* path, openmode mode) -> filebuf*
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
        auto oflags = O_TMPFILE;
        if (mode & (in|out)) {
            oflags |= O_RDWR;
        }
        else if (mode & in) {
            oflags |= O_RDONLY;
        }
        else if (mode & out) {
            oflags |= O_WRONLY;
        }
        if (mode & trunc) {
            oflags |= O_TRUNC;
        }
        if (mode & app) {
            oflags |= O_APPEND;
        }
        if (mode & noreplace) {
            oflags |= O_EXCL;
        }
        if (mode & cloexec) {
            oflags |= O_CLOEXEC;
        }
        const auto fd = ::open( // NOLINT(cppcoreguidelines-pro-type-vararg)
                               path, oflags, 0666);
        if (fd == -1) {
            return nullptr;
        }
        new_fp = std::unique_ptr<FILE, fcloser>{::fdopen(fd, mode_cstr)};
        if (!new_fp) {
            ::close(fd);
            return nullptr;
        }
#else
        new_fp = std::unique_ptr<FILE, fcloser>{std::tmpfile()};
#endif
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

}
