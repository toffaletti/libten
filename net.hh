#include "descriptors.hh"

namespace fw {

int dial(int fd, const char *addr, uint16_t port);

class netsock : boost::noncopyable {
public:
    netsock(int fd) throw (errno_error);

    netsock(int domain, int type, int protocol=0) throw (errno_error);

    void bind(address &addr) throw (errno_error) { s.bind(addr); }
    void listen(int backlog=128) throw (errno_error) { s.listen(backlog); }
    bool getpeername(address &addr) throw (errno_error) __attribute__((warn_unused_result)) {
        return s.getpeername(addr);
    }
    void getsockname(address &addr) throw (errno_error) {
        return s.getsockname(addr);
    }
    template <typename T> void getsockopt(int level, int optname,
        T &optval, socklen_t &optlen) throw (errno_error)
    {
        return s.getsockopt(level, optname, optval, optlen);
    }
    template <typename T> void setsockopt(int level, int optname,
        const T &optval, socklen_t optlen) throw (errno_error)
    {
        return s.setsockopt(level, optname, optval, optlen);
    }
    template <typename T> void setsockopt(int level, int optname,
        const T &optval) throw (errno_error)
    {
        return s.setsockopt(level, optname, optval);
    }

    //! dial requires a large 8MB stack size for getaddrinfo
    int dial(const char *addr,
            uint16_t port, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

    int connect(const address &addr,
            unsigned int timeout_ms=0) __attribute__((warn_unused_result));

    int accept(address &addr,
            int flags=0, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

    ssize_t recv(void *buf,
            size_t len, int flags=0, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

    ssize_t send(const void *buf,
            size_t len, int flags=0, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

    void close() { s.close(); }

    socket_fd s;
};

} // end namespace fw

