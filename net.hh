#ifndef NETSOCK_HH
#define NETSOCK_HH

#include "descriptors.hh"
#include "task.hh"

namespace ten {

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

class netsock_server {
public:
    netsock_server(const std::string &protocol_name_,
            size_t stacksize_=default_stacksize,
            int timeout_ms_=-1)
        : sock(AF_INET, SOCK_STREAM),
        protocol_name(protocol_name_),
        stacksize(stacksize_),
        timeout_ms(timeout_ms_)
    {
        sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    }

    netsock_server(const netsock_server &) = delete;
    netsock_server &operator=(const netsock_server &) = delete;

    //! listen and accept connections
    void serve(const std::string &ipaddr, uint16_t port) {
        address baddr(ipaddr.c_str(), port);
        sock.bind(baddr);
        sock.getsockname(baddr);
        LOG(INFO) << "listening for " << protocol_name << " on " << baddr;
        sock.listen();
        try {
            for (;;) {
                address client_addr;
                int fd;
                while ((fd = sock.accept(client_addr, 0)) > 0) {
                    taskspawn(std::bind(&netsock_server::client_task, this, fd), stacksize);
                }
            }
        } catch (...) {
            // do any cleanup to free memory etc...
            // for example if your callbacks hold a reference to this server
            // this would be the place to release that circular reference
            on_shutdown();
            throw;
        }
    }

protected:
    netsock sock;
    std::string protocol_name;
    size_t stacksize;
    int timeout_ms;

    virtual void on_shutdown() {}
    virtual void on_connection(netsock &s) = 0;

    void client_task(int fd) {
        netsock s(fd);
        try {
            on_connection(s);
        } catch (std::exception &e) {
            LOG(ERROR) << "unhandled client task error: " << e.what();
        }
    }
};

} // end namespace ten

#endif // NETSOCK_HH
