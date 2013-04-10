#ifndef LIBTEN_DESCRIPTORS_HH
#define LIBTEN_DESCRIPTORS_HH

#include "error.hh"
#include "net/address.hh"
#include "logging.hh"

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <signal.h>
#include <vector>

namespace ten {

inline bool io_not_ready(int e = errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
    return (e == EAGAIN) || (e == EWOULDBLOCK);
#else
    return (e == EAGAIN);
#endif
}

//! \file
//! contains wrappers around most fd based apis

//! base class for other file descriptors
//
//! contains methods common to most file descriptors
//! in C++0x this should be movable, but not copyable
//! close is called in destructor
struct fd_base {
    //! the file descriptor
    int fd;

    //! \param fd_ the file descriptor
    fd_base(int fd_=-1) : fd(fd_) {}

    fd_base(const fd_base &) = delete;
    fd_base &operator =(const fd_base &) = delete;

    fd_base(fd_base &&other) {
        fd = other.fd;
        other.fd = -1;
    }
    fd_base &operator = (fd_base &&other) {
        if (this != &other) {
            if (valid()) close();
            std::swap(fd, other.fd);
        }
        return *this;
    }

    friend bool operator == (const fd_base &fb, int fd_) { return fb.fd == fd_; }
    friend bool operator == (int fd_, const fd_base &fb) { return fb.fd == fd_; }

    //! protect against accidental boolean comparison
    template <class T>
    friend typename std::enable_if<std::is_same<T, bool>::value, bool>::type
    operator == (const fd_base &fb, T t) {
        static_assert(!std::is_same<T, bool>::value, "fd == bool");
    }

    //! protect against accidental boolean comparison
    template <class T>
    friend typename std::enable_if<std::is_same<T, bool>::value, bool>::type
    operator == (T t, const fd_base &fb) {
        static_assert(!std::is_same<T, bool>::value, "fd == bool");
    }

    int fcntl(int cmd)              { return ::fcntl(fd, cmd); }
    int fcntl(int cmd, int arg)     { return ::fcntl(fd, cmd, arg); }
    int fcntl(int cmd, struct flock *lock) { return ::fcntl(fd, cmd, lock); }

    //! make fd nonblocking by setting O_NONBLOCK flag
    void setblock(bool block = true) {
        int flags;
        throw_if((flags = fcntl(F_GETFL)) == -1, "fcntl(F_GETFL)");
        if (block)
            flags &= O_NONBLOCK;
        else
            flags |= O_NONBLOCK;
        throw_if(fcntl(F_SETFL, flags) == -1, "fcntl(F_SETFL)");
    }
    void setnonblock() {
        setblock(false);
    }

    //! true if fd != -1
    bool valid() const { return fd != -1; }

    //! read from fd
    ssize_t read(void *buf, size_t count) noexcept __attribute__((warn_unused_result)) {
        return ::read(fd, buf, count);
    }

    //! write to fd
    ssize_t write(const void *buf, size_t count) noexcept __attribute__((warn_unused_result)) {
        return ::write(fd, buf, count);
    }

    //! wrapper around readv
    ssize_t readv(const struct iovec *iov, int iovcnt) noexcept __attribute__((warn_unused_result)) {
        return ::readv(fd, iov, iovcnt);
    }

    //! wrapper around writev
    ssize_t writev(const struct iovec *iov, int iovcnt) noexcept __attribute__((warn_unused_result)) {
        return ::writev(fd, iov, iovcnt);
    }

    //! attempts to close fd
    void close() noexcept {
        // man 2 close says close can result in EBADF, EIO, and EINTR
        // Linux and any thread-safe OS will not return EINTR, but we check just in case
        if (::close(fd) == -1) {
            if (errno == EINTR) {
                saved_backtrace bt;
                LOG(DFATAL) << "close() failed with EINTR; This Should Never Happen\n" << bt.str();
            }
        }
        fd = -1;
    }

    //! attempts to close fd if it is valid
    //! see close() for when this can throw
    ~fd_base() {
        if (valid()) close();
    }
};

//! eventfd create a file descriptor for event notification
struct event_fd : fd_base {
    event_fd(unsigned int initval=0, int flags=0) {
        fd = ::eventfd(initval, flags | EFD_CLOEXEC);
        throw_if(fd == -1);
    }

    void write(uint64_t val) {
        throw_if(fd_base::write(&val, sizeof(val)) == -1);
    }

    uint64_t read() {
        uint64_t val;
        throw_if(fd_base::read(&val, sizeof(val)) == -1);
        return val;
    }
};

//! pipe_fd is a pair of unidirectional file descriptors
struct pipe_fd {
    //! read end of the pipe
    fd_base r;
    //! write end of the pipe
    fd_base w;

    //! create a pair of unidirectional file descriptors with
    //! pipe2() that can be used for interprocess communication
    //! see man 2 pipe2 for more info
    //! \param flags can be 0, or ORed O_NONBLOCK, O_CLOEXEC
    pipe_fd(int flags=0) {
        int fds[2];
        throw_if(pipe2(fds, flags | O_CLOEXEC) == -1);
        r.fd = fds[0];
        w.fd = fds[1];
    }

    //! read from the read fd end of the pipe
    ssize_t read(void *buf, size_t count) noexcept __attribute__((warn_unused_result)) {
        return r.read(buf, count);
    }

    //! write to the write fd end of the pipe
    ssize_t write(const void *buf, size_t count) noexcept __attribute__((warn_unused_result)) {
        return w.write(buf, count);
    }

    void setnonblock() {
        r.setnonblock();
        w.setnonblock();
    }
};

//! wrapper around file-based file descriptor
struct file_fd : fd_base {
    //! calls open to create file descriptor
    file_fd(const char *pathname, int flags, mode_t mode) {
        fd = ::open(pathname, flags | O_CLOEXEC, mode);
    }

    //! read from a given offset
    ssize_t pread(void *buf, size_t count, off_t offset) __attribute__((warn_unused_result)) {
        return ::pread(fd, buf, count, offset);
    }

    //! write to a given offset
    ssize_t pwrite(const void *buf, size_t count, off_t offset) __attribute__((warn_unused_result)) {
        return ::pwrite(fd, buf, count, offset);
    }

    //! repositions the offset of the file descriptor
    //! \param offset the new offset according to the directive whence
    //! \param whence one of SEEK_SET, SEEK_CUR, SEEK_END
    off_t lseek(off_t offset, int whence) __attribute__((warn_unused_result)) {
        return ::lseek(fd, offset, whence);
    }
};

//! wrapper around an epoll fd
struct epoll_fd : fd_base {

    //! creates fd with epoll_create
    epoll_fd() {
        // size is ignored since linux kernel 2.6.8, but still must be positive
        fd = epoll_create1(O_CLOEXEC);
        throw_if(fd == -1);
    }

    //! register the target file descriptor with the epoll instance
    int add(int fd_, struct epoll_event &event) {
        return ::epoll_ctl(fd, EPOLL_CTL_ADD, fd_, &event);
    }

    //! change the event associated with the target file descriptor
    int modify(int fd_, struct epoll_event &event) {
        return ::epoll_ctl(fd, EPOLL_CTL_MOD, fd_, &event);
    }

    //! deregister the target file descriptor from the epoll instance
    //! NOTE: closing a file descriptor will automatically remove it from all epoll instances
    int remove(int fd_) {
        return ::epoll_ctl(fd, EPOLL_CTL_DEL, fd_, NULL);
    }

    //! \param events an array of epoll_events structs to contain events available to the caller
    //! \param timeout milliseconds to wait for events, -1 waits indefinitely
    void wait(std::vector<epoll_event> &events, int timeout=-1) {
        int s = ::epoll_wait(fd, &events[0], events.size(), timeout);
        // if EINTR, the caller might need to do something else
        // and then call wait again. so don't throw exception
        if (s == -1 && errno == EINTR) s=0;
        // XXX: the failures for epoll_wait are all fatal. 
        // this is perhaps flawed because it is not a clean shutdown
        // however, clean shutdown might be difficult without epoll...
        PCHECK(s >= 0) << "epoll_wait failed";
        events.resize(s);
    }
};


//! light wrapper around socket file descriptor
struct socket_fd : fd_base {
    //! creates fd with socket()
    socket_fd(int domain, int type, int protocol=0) {
        fd = ::socket(domain, type | SOCK_CLOEXEC, protocol);
        throw_if(fd == -1);
    }

    //! create a socket_fd from an already existing file descriptor
    //! used for accept() and socketpair()
    socket_fd(int fd_) noexcept : fd_base(fd_) {}

    //! bind socket to address
    void bind(const address &addr) {
        throw_if(::bind(fd, addr.sockaddr(), addr.addrlen()) == -1);
    }

    //! marks the socket as passive, to be used to accept incoming connections using accept()
    //! \param backlog the queue length for completely established connections waiting to be accepted
    void listen(int backlog=128) {
        throw_if(::listen(fd, backlog) == -1);
    }

    //! connects fd to the address specified by addr
    int connect(const address &addr) __attribute__((warn_unused_result)) {
        return ::connect(fd, addr.sockaddr(), addr.addrlen());
    }

    //! wrapper around shutdown()
    int shutdown(int how) __attribute__((warn_unused_result)) {
        return ::shutdown(fd, how);
    }

    //! wrapper around recv()
    ssize_t recv(void *buf, size_t len, int flags=0) __attribute__((warn_unused_result)) {
        return ::recv(fd, buf, len, flags);
    }

    //! wrapper around send()
    ssize_t send(const void *buf, size_t len, int flags=0) __attribute__((warn_unused_result)) {
        return ::send(fd, buf, len, flags);
    }

    //! wrapper around recv()
    ssize_t recvfrom(void *buf, size_t len, address &addr, int flags=0) __attribute__((warn_unused_result)) {
        socklen_t addrlen = addr.maxlen();
        return ::recvfrom(fd, buf, len, flags, addr.sockaddr(), &addrlen);
    }

    //! wrapper around send()
    ssize_t sendto(const void *buf, size_t len, address &addr, int flags=0) __attribute__((warn_unused_result)) {
        return ::sendto(fd, buf, len, flags, addr.sockaddr(), addr.addrlen());
    }

    //! \param addr returns the address of the peer connected to the socket fd
    //! \return true on success, false if socket is not connected
    bool getpeername(address &addr) __attribute__((warn_unused_result)) {
        socklen_t addrlen = addr.maxlen();
        if (::getpeername(fd, addr.sockaddr(), &addrlen) == 0) {
            return true;
        } else if (errno != ENOTCONN) {
            throw errno_error("getpeername");
        }
        addr.clear();
        // return false if socket is not connected
        return false;
    }

    //! \param addr returns the address to which the socket fd is bound
    void getsockname(address &addr) {
        socklen_t addrlen = addr.maxlen();
        throw_if(::getsockname(fd, addr.sockaddr(), &addrlen) == -1);
    }

    //! wrapper around getsockopt()
    template <typename T> void getsockopt(int level, int optname,
        T &optval, socklen_t &optlen)
    {
        throw_if(::getsockopt(fd, level, optname, &optval, &optlen) == -1);
    }
    //! template version of getsockopt that frees you from specifying optlen for basic types
    template <typename T> void getsockopt(int level, int optname,
        T &optval)
    {
        socklen_t optlen = sizeof(optval);
        socket_fd::getsockopt(level, optname, optval, optlen);
    }

    //! wrapper around setsockopt()
    template <typename T> void setsockopt(int level, int optname,
        const T &optval, socklen_t optlen)
    {
        throw_if(::setsockopt(fd, level, optname, &optval, optlen) == -1);
    }
    //! template version of setsockopt that frees you from specifying optlen for basic types
    template <typename T> void setsockopt(int level, int optname,
        const T &optval)
    {
        socket_fd::setsockopt(level, optname, optval, sizeof(optval));
    }

    //! wrapper around socketpair()
    static std::pair<socket_fd, socket_fd> pair(int domain, int type, int protocol=0) {
        int sv[2];
        throw_if(::socketpair(domain, type | SOCK_CLOEXEC, protocol, sv) == -1);
        return std::make_pair(sv[0], sv[1]);
    }

    //! \param addr returns the address of the peer socket
    //! \param flags 0 or xor of SOCK_NONBLOCK, SOCK_CLOEXEC
    //! \return the file descriptor for the peer socket or -1 on error
    int accept(address &addr, int flags=0) __attribute__((warn_unused_result)) {
        socklen_t addrlen = addr.maxlen();
        return ::accept4(fd, addr.sockaddr(), &addrlen, flags | SOCK_CLOEXEC);
    }

    //! print socket file descriptor number
    friend std::ostream &operator << (std::ostream &out, const socket_fd &s) {
        out << "socket_fd(" << s.fd << ")";
        return out;
    }
};

//! wrapper around timerfd
struct timer_fd : fd_base {
    //! creates fd with timerfd_create()
    timer_fd(int clockid=CLOCK_MONOTONIC, int flags=0) {
        fd = timerfd_create(clockid, flags | O_CLOEXEC);
        throw_if(fd == -1);
    }

    //! arms or disarms the timer referred to by fd
    void settime(const struct itimerspec &new_value,
            struct itimerspec &old_value, int flags = 0)
    {
        throw_if(timerfd_settime(fd, flags, &new_value, &old_value) == -1);
    }

    //! arms or disarms the timer referred to by fd
    void settime(const struct itimerspec &new_value, int flags = 0) {
        throw_if(timerfd_settime(fd, flags, &new_value, nullptr) == -1);
    }

    //! \param curr_value will contain the current value of the timer
    void gettime(struct itimerspec &curr_value) {
        throw_if(timerfd_gettime(fd, &curr_value) == -1);
    }

    //! return the number of times the timer has fired
    uint64_t read() {
        uint64_t val;
        throw_if(fd_base::read(&val, sizeof(val)) == -1);
        return val;
    }

};

//! wrapper around signalfd
struct signal_fd : fd_base {
    //! create fd with signalfd()
    //! also calls sigprocmask() to block the signals in mask
    signal_fd(const sigset_t &mask, int flags=0) {
        throw_if(::sigprocmask(SIG_BLOCK, &mask, NULL) == -1);
        fd = ::signalfd(-1, &mask, flags | O_CLOEXEC);
        throw_if(fd == -1);
    }

    //! convenience method for reading struct siginalfd_siginfo
    void read(struct signalfd_siginfo &siginfo) {
        ssize_t r = fd_base::read(&siginfo, sizeof(siginfo));
        throw_if(r == -1);
    }
};

//! wrapper around inotify
struct inotify_fd : fd_base {
    inotify_fd(int flags = 0) {
        fd = ::inotify_init1(flags | IN_CLOEXEC);
        throw_if(fd == -1);
    }

    int add_watch(const char *pathname, uint32_t mask) {
        return ::inotify_add_watch(fd, pathname, mask);
    }

    int rm_watch(int wd) {
        return ::inotify_rm_watch(fd, wd);
    }

    ssize_t read(inotify_event &event) {
        return fd_base::read(&event, sizeof(event));
    }
};

} // end namespace ten

#endif // LIBTEN_DESCRIPTORS_HH
