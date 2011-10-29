#include "net.hh"
#include "ioproc.hh"
#include <netdb.h>

namespace ten {

int netconnect(int fd, const address &addr, unsigned int ms) {
    while (::connect(fd, addr.sockaddr(), addr.addrlen()) < 0) {
        if (errno == EINTR)
            continue;
        if (errno == EINPROGRESS || errno == EADDRINUSE) {
            errno = 0;
            if (fdwait(fd, 'w', ms)) {
                return 0;
            } else if (errno == 0) {
                errno = ETIMEDOUT;
            }
        }
        return -1;
    }
    return 0;
}

int dial(int fd, const char *addr, uint16_t port) {
    struct addrinfo *results = 0;
    struct addrinfo *result = 0;
    int status = getaddrinfo(addr, NULL, NULL, &results);
    if (status == 0) {
        for (result = results; result != NULL; result = result->ai_next) {
            address addr(result->ai_addr, result->ai_addrlen);
            addr.port(port);
            status = netconnect(fd, addr, 0);
            if (status == 0) break;
        }
    }
    freeaddrinfo(results);
    return status;
}

netsock::netsock(int fd) throw (errno_error) : s(fd) {}

netsock::netsock(int domain, int type, int protocol) throw (errno_error)
    : s(domain, type | SOCK_NONBLOCK, protocol)
{
}

int netsock::dial(const char *addr, uint16_t port, unsigned int timeout_ms) {
    // need large stack size for getaddrinfo (see dial)
    // TODO: maybe replace with c-ares from libcurl project
    ioproc io(8*1024*1024);
    return iodial(io, s.fd, addr, port);
}

int netsock::connect(const address &addr, unsigned int timeout_ms) {
    return netconnect(s.fd, addr, timeout_ms);
}

int netsock::accept(address &addr, int flags, unsigned int timeout_ms) {
    int fd;
    while ((fd = s.accept(addr, flags | SOCK_NONBLOCK)) < 0) {
        if (errno == EINTR)
            continue;
        if (!IO_NOT_READY_ERROR)
            return -1;
        if (!fdwait(s.fd, 'r', timeout_ms)) {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    return fd;
}

ssize_t netsock::recv(void *buf, size_t len, int flags, unsigned int timeout_ms) {
    ssize_t nr;
    while ((nr = s.recv(buf, len, flags)) < 0) {
        if (errno == EINTR)
            continue;
        if (!IO_NOT_READY_ERROR)
            break;
        if (!fdwait(s.fd, 'r', timeout_ms)) {
            errno = ETIMEDOUT;
            break;
        }
    }
    return nr;
}

ssize_t netsock::send(const void *buf, size_t len, int flags, unsigned int timeout_ms) {
    size_t total_sent=0;
    while (total_sent < len) {
        ssize_t nw = s.send(&((const char *)buf)[total_sent], len-total_sent, flags);
        if (nw == -1) {
            if (errno == EINTR)
                continue;
            if (!IO_NOT_READY_ERROR)
                return -1;
            if (!fdwait(s.fd, 'w', timeout_ms)) {
                errno = ETIMEDOUT;
                return total_sent;
            }
        } else {
            total_sent += nw;
        }
    }
    return total_sent;
}

} // end namespace ten

