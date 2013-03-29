#include "ten/net.hh"

static void set_errno_from(int fd, int default_err) {
    int e = default_err;
    socklen_t len = sizeof e;
    (void)::getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &len);
    errno = e;
}

namespace ten {

// on timeout, caller should close, since the kernel may still be trying to connect
int netconnect(int fd, const address &addr, optional_timeout ms) {
    while (::connect(fd, addr.sockaddr(), addr.addrlen()) < 0) {
        if (errno == EINTR)
            continue;
        if (errno == EINPROGRESS || errno == EADDRINUSE) {
            errno = 0;
            if (fdwait(fd, 'w', ms)) {
                return 0;
            }
            set_errno_from(fd, ETIMEDOUT);
        }
        return -1;
    }
    return 0;
}

int netaccept(int fd, address &addr, int flags, optional_timeout timeout_ms) {
    int nfd;
    socklen_t addrlen = addr.maxlen();
    while ((nfd = ::accept4(fd, addr.sockaddr(), &addrlen, flags | SOCK_NONBLOCK)) < 0) {
        if (errno == EINTR)
            continue;
        if (!io_not_ready())
            return -1;
        if (!fdwait(fd, 'r', timeout_ms)) {
            set_errno_from(fd, ETIMEDOUT);
            return -1;
        }
    }
    return nfd;
}

ssize_t netrecv(int fd, void *buf, size_t len, int flags, optional_timeout timeout_ms) {
    ssize_t nr;
    while ((nr = ::recv(fd, buf, len, flags)) < 0) {
        if (errno == EINTR)
            continue;
        if (!io_not_ready())
            break;
        if (!fdwait(fd, 'r', timeout_ms)) {
            set_errno_from(fd, ETIMEDOUT);
            break;
        }
    }
    return nr;
}

ssize_t netsend(int fd, const void *buf, size_t len, int flags, optional_timeout timeout_ms) {
    size_t total_sent=0;
    while (total_sent < len) {
        ssize_t nw = ::send(fd, &((const char *)buf)[total_sent], len-total_sent, flags);
        if (nw == -1) {
            if (errno == EINTR)
                continue;
            if (!io_not_ready()) {
                if (total_sent)
                    return total_sent;
                else
                    return -1;
            }
            if (!fdwait(fd, 'w', timeout_ms)) {
                if (total_sent)
                    return total_sent;
                else {
                    set_errno_from(fd, ETIMEDOUT);
                    return -1;
                }
            }
        } else {
            total_sent += nw;
        }
    }
    return total_sent;
}

void netsock::dial(const char *addr, uint16_t port, optional_timeout timeout_ms) {
    netdial(s.fd, addr, port, timeout_ms);
}

} // end namespace ten

