#include "net.hh"
#include "ioproc.hh"

namespace fw {

netsock::netsock(int fd) throw (errno_error) : s(fd) {}

netsock::netsock(int domain, int type, int protocol) throw (errno_error)
    : s(domain, type | SOCK_NONBLOCK, protocol)
{
}

int netsock::dial(const char *addr, uint16_t port, unsigned int timeout_ms) {
    auto io = ioproc(8*1024*1024);
    return iodial(io, s.fd, addr, port);
}

int netsock::connect(const address &addr, unsigned int timeout_ms) {
    while (s.connect(addr) < 0) {
        if (errno == EINTR)
            continue;
        if (errno == EINPROGRESS || errno == EADDRINUSE) {
            errno = 0;
            if (fdwait(s.fd, 'w', timeout_ms)) {
                return 0;
            } else if (errno == 0) {
                errno = ETIMEDOUT;
            }
        }
        return -1;
    }
    return 0;
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

} // end namespace fw

