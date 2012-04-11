#ifndef TEN_IO_HH
#define TEN_IO_HH

#include <string>
#include <sys/uio.h>
#include "error.hh"

namespace ten {
namespace io {

class reader {
public:

    reader() {}

    //! readers are not copyable
    reader(const reader &other) = delete;
    reader &operator =(const reader &other) = delete;

    size_t read(char *buf, size_t n) __attribute__((warn_unused_result))
    {
        struct iovec iov[] = {{(void *)buf, n}};
        return readv(iov, 1);
    }

    virtual size_t readv(const struct iovec *iov, int iovcnt)
        __attribute__((warn_unused_result)) = 0;

    virtual ~reader() {}
};

class writer {
public:
    writer() {}

    //! writers are not copyable
    writer(const writer &other) = delete;
    writer &operator =(const writer &other) = delete;
    
    size_t write(const std::string &s) __attribute__((warn_unused_result))
    {
        return write(s.c_str(), s.size());
    }

    size_t write(const char *buf, size_t n) __attribute__((warn_unused_result))
    {
        const struct iovec iov[] = {{(void *)buf, n}};
        return writev(iov, 1);
    }

    virtual size_t writev(const struct iovec *iov, int iovcnt)
        __attribute__((warn_unused_result)) = 0;

    virtual ~writer() {}
};

class fwriter_base : public io::writer {
public:
    explicit fwriter_base(FILE *f_=0) : f(f_) {}

    virtual ~fwriter_base() {
        close();
    }

    virtual size_t writev(const struct iovec *iov, int iovcnt)
        __attribute__((warn_unused_result))
    {
        size_t ret = 0;
        for (int i=0; i<iovcnt; ++i) {
            if (!fwrite(iov[i].iov_base, iov[i].iov_len, 1, f)) {
                return ferror(f);
            }
            ret += iov[i].iov_len;
        }
        return ret;
    }

    void flush() {
        THROW_ON_NONZERO_ERRNO(fflush(f));
    }

    void close() {
        if (f) {
            THROW_ON_NONZERO_ERRNO(fclose(f));
            f = 0;
        }
    }

protected:
    FILE *f;
};

class memstream : public io::fwriter_base {
public:
    memstream() : _ptr(0), _size(0) {
        fwriter_base::f = open_memstream(&_ptr, &_size);
    }

    //! must flush before calling this
    size_t size() {
        return _size;
    }

    //! must flush before calling this
    const char *ptr() {
        return _ptr;
    }

    virtual ~memstream() {
        close();
        free(_ptr);
    }

private:
    char *_ptr;
    size_t _size;
};

class fd_base : public io::reader, public io::writer {
public:
    explicit fd_base(int fd_=-1) : fd(fd_) {}

    fd_base(fd_base &&other) : fd(-1) {
        std::swap(fd, other.fd);
    }

    fd_base &operator = (fd_base &&other) {
        if (this != &other) {
            std::swap(fd, other.fd);
        }
        return *this;
    }

    //! true if fd != -1
    bool valid() { return fd != -1; }

    //! attempts to close fd, it will retry on EINTR
    //! and throw errno_error for all other errors (EIO, EBADF)
    void close() {
        // man 2 close says close can result in EBADF, EIO, and EINTR
        while (::close(fd) == -1) {
            switch (errno) {
                case EINTR: // just retry
                    continue;
                case EIO: // catastrophic
                case EBADF: // programming error
                default:
                    throw errno_error();
            }
        }
        fd = -1;
    }

    bool operator == (int fd_) const { return fd == fd_; }

    int fcntl(int cmd) { return ::fcntl(fd, cmd); }
    int fcntl(int cmd, long arg) { return ::fcntl(fd, cmd, arg); }
    int fcntl(int cmd, flock *lock) { return ::fcntl(fd, cmd, lock); }

    //! make fd nonblocking by setting O_NONBLOCK flag
    void setblock(bool block = true) {
        int flags;
        THROW_ON_ERROR((flags = fcntl(F_GETFL)));
        if (block)
            flags &= O_NONBLOCK;
        else
            flags |= O_NONBLOCK;
        THROW_ON_ERROR(fcntl(F_SETFL, flags));
    }

    void setnonblock() {
        setblock(false);
    }

    virtual size_t readv(const struct iovec *iov, int iovcnt)
        __attribute__((warn_unused_result))
    {
        return ::readv(fd, iov, iovcnt);
    }

    virtual size_t writev(const struct iovec *iov, int iovcnt)
        __attribute__((warn_unused_result))
    {
        return ::writev(fd, iov, iovcnt);
    }

    //! attempts to close fd if it is valid
    //! see close() for when this can throw
    virtual ~fd_base() {
        if (valid()) { close(); }
    }
private:
    int fd;
};

} // namespace io
} // namespace ten

#endif
