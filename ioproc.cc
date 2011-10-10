#include "ioproc.hh"
#include "channel.hh"
#include "address.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <cassert>

namespace fw {

struct _ioproc {
    uint64_t tid;
    op_func op;
    bool inuse;
    channel<_ioproc *> c;
    channel<_ioproc *> creply;
    va_list arg;
    intptr_t ret;
    char err[256];

    _ioproc() : inuse(false), ret(0) {
    }

    ~_ioproc() {
        // tell proc/thread to exit
        c.send(0);
    }
};

static void iotask(iop io) {

    tasksetname("ioproc%p", io.get());
    for (;;) {
        _ioproc *x = io->c.recv();
        if (x == 0) break;
        assert(x == io.get());

        io->ret = io->op(io->arg);
        if (io->ret < 0) {
            strerror_r(errno, io->err, sizeof(io->err));
        }
        io->creply.send(x);
    }
}

iop ioproc(size_t stacksize) {
    iop p(new _ioproc);
    p->tid = procspawn(std::bind(iotask, p), stacksize);
    return p;
}

intptr_t iocall(iop &io, const op_func &op, ...) {
    assert(!io->inuse);
    io->inuse = true;
    io->op = op;
    // start arg parsing, will resume in op_func
    // which is called in another thread
    va_start(io->arg, op);
    // pass control of ioproc struct to thread
    // that will make the syscall
    io->c.send(io.get());
    // wait for thread to finish and pass
    // control of the ioproc struct back
    _ioproc *x = io->creply.recv();
    assert(x == io.get());
    io->inuse = false;
    va_end(io->arg);
    return io->ret;
}

static intptr_t _ioopen(va_list &arg) {
    char *path;
    int mode;
    path = va_arg(arg, char *);
    mode = va_arg(arg, int);
    return ::open(path, mode);
}

int ioopen(iop &io, char *path, int mode) {
    return iocall(io, _ioopen, path, mode);
}

static intptr_t _ioclose(va_list &arg) {
    int fd;
    fd = va_arg(arg, int);
    return ::close(fd);
}

int ioclose(iop &io, int fd) {
    return iocall(io, _ioclose, fd);
}

static intptr_t _ioread(va_list &arg) {
    int fd;
    void *buf;
    size_t n;
    fd = va_arg(arg, int);
    buf = va_arg(arg, void*);
    n = va_arg(arg, size_t);
    return ::read(fd, buf, n);
}

ssize_t ioread(iop &io, int fd, void *buf, size_t n) {
    return iocall(io, _ioread, fd, buf, n);
}

static intptr_t _iowrite(va_list &arg) {
    int fd;
    void *a;
    size_t n;

    fd = va_arg(arg, int);
    a = va_arg(arg, void*);
    n = va_arg(arg, long);
    return ::write(fd, a, n);
}

ssize_t iowrite(iop &io, int fd, void *a, size_t n) {
    return iocall(io, _iowrite, fd, a, n);
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

intptr_t _iodial(va_list &arg) {
    int fd = va_arg(arg, int);
    const char *addr = va_arg(arg, const char *);
    uint16_t port = va_arg(arg, int);
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

int iodial(iop &io, int fd, const char *addr, uint16_t port) {
    return iocall(io, _iodial, fd, addr, port);
}

} // end namespace fw
