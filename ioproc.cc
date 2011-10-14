#include "ioproc.hh"
#include "address.hh"
#include "logging.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <cassert>

namespace fw {

namespace detail {
static void iotask(channel<ioproc *> c) {
    ioproc *io = c.recv();
    tasksetname("ioproc%p", io);
    for (;;) {
        ioproc *x = c.recv();
        DVLOG(5) << "iorpoc got " << x;
        if (x == 0) break;
        assert(x == io);

        errno = 0;
        if (io->vop) {
            io->vop();
        } else if (io->op) {
            io->ret = io->op();
        } else {
            abort();
        }
        // capture error string
        if (errno) {
            strerror_r(errno, io->err, sizeof(io->err));
        }
        io->creply.send(x);
    }
}

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

int dial(int fd, const char *addr, uint64_t port) {
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

} // end namespace detail

shared_ioproc ioproc(size_t stacksize) {
    shared_ioproc p(new detail::ioproc);
    p->tid = procspawn(std::bind(detail::iotask, p->c), stacksize);
    p->c.send(p.get());
    return p;
}

detail::ioproc *ioproc_pool::new_resource(size_t stacksize) {
    std::unique_ptr<detail::ioproc> p (new detail::ioproc);
    p->tid = procspawn(std::bind(detail::iotask, p->c), stacksize);
    p->c.send(p.get());
    return p.release();
}

void ioproc_pool::free_resource(detail::ioproc *p) {
    delete p;
}

} // end namespace fw
