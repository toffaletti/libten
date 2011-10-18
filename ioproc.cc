#include "ioproc.hh"
#include "address.hh"
#include "logging.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>

namespace fw {

namespace detail {

void iotask(channel<shared_ioproc> &c) {
    // TODO: system task?
    try {
        shared_ioproc io = c.recv();
        ioproc *x = io.get();
        io.reset();
        taskname("ioproc%p", x);
        for (;;) {
            taskstate("waiting");
            io = c.recv();
            DVLOG(5) << "iorpoc got " << io.get() << " rc: " << io.use_count();
            if (!io) break;
            assert(x == io.get());

            taskstate("executing");
            errno = 0;
            if (io->vop) {
                io->vop();
                io->vop = std::function<void ()>();
            } else if (io->op) {
                io->ret = io->op();
                io->op = std::function<int ()>();
            } else {
                abort();
            }
            // capture error string
            if (errno) {
                strerror_r(errno, io->err, sizeof(io->err));
            }
            taskstate("replying");
            io->inuse = false;
            io->creply.send(x);
            io.reset();
        }
    } catch (channel_closed_error &e) {
        taskstate("channel closed");
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

shared_ioproc ioproc(size_t stacksize, void (*iotask)(channel<shared_ioproc> &)) {
    shared_ioproc p(new detail::ioproc);
    p->tid = procspawn(std::bind(iotask, p->c), stacksize);
    p->c.send(p);
    return p;
}

} // end namespace fw
