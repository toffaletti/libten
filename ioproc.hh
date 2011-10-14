#ifndef IOPROC_HH
#define IOPROC_HH

#include "task.hh"
#include "channel.hh"
#include <stdarg.h>
#include <memory>
#include <boost/any.hpp>
#include "shared_pool.hh"
#include <fcntl.h>

namespace fw {

namespace detail {
struct ioproc {
    uint64_t tid;
    std::function<void ()> vop;
    std::function<boost::any ()> op;
    channel<ioproc *> c;
    channel<ioproc *> creply;
    char err[256];
    bool inuse;
    boost::any ret;

    ioproc() : inuse(false), ret(0) {
    }

    ~ioproc() {
        // tell proc/thread to exit
        c.send(0);
    }
};

inline void magic(ioproc *io) {
    io->inuse = true;
    // pass control of ioproc struct to thread
    // that will make the syscall
    io->c.send(io);
    // wait for thread to finish and pass
    // control of the ioproc struct back
    detail::ioproc *x = io->creply.recv();
    assert(x == io);
    io->inuse = false;
}

// TODO: doesn't really belong here, maybe net.hh
int dial(int fd, const char *addr, uint64_t port);

}

typedef std::shared_ptr<detail::ioproc> shared_ioproc;

shared_ioproc ioproc(size_t stacksize=default_stacksize);

template <typename ReturnT, typename ProcT> ReturnT iocall(ProcT &io, const std::function<boost::any ()> &op) {
    assert(!io->inuse);
    assert(!io->vop);
    assert(!io->op);
    io->op = op;
    detail::magic(io.get());
    // make empty
    io->op = std::function<int ()>();
    return boost::any_cast<ReturnT>(io->ret);
}

template <typename ProcT> void iocall(ProcT &io, const std::function<void ()> &vop) {
    assert(!io->inuse);
    assert(!io->vop);
    assert(!io->op);
    io->vop = vop;
    detail::magic(io.get());
    // make empty
    io->vop = std::function<void ()>();
}

template <typename ProcT> int ioopen(ProcT &io, char *path, int mode) {
    return iocall<int>(io, std::bind((int (*)(char *, int))::open, path, mode));
}

template <typename ProcT>  int ioclose(ProcT &io, int fd) {
    return iocall<int>(io, std::bind(::close, fd));
}

template <typename ProcT> ssize_t ioread(ProcT &io, int fd, void *buf, size_t n) {
    return iocall<ssize_t>(io, std::bind(::read, fd, buf, n));
}

template <typename ProcT> ssize_t iowrite(ProcT &io, int fd, void *buf, size_t n) {
    return iocall<ssize_t>(io, std::bind(::write, fd, buf, n));
}

template <typename ProcT> int iodial(ProcT &io, int fd, const char *addr, uint64_t port) {
    return iocall<int>(io, std::bind(detail::dial, fd, addr, port));
}

class ioproc_pool : public shared_pool<detail::ioproc> {
private:
    static detail::ioproc *new_resource(size_t stacksize);
    static void free_resource(detail::ioproc *p);
public:
    ioproc_pool(size_t stacksize_=default_stacksize, ssize_t max_=-1)
        : shared_pool<detail::ioproc>("ioproc_pool",
        std::bind(new_resource, stacksize_),
        ioproc_pool::free_resource,
        max_) {}
};

} // end namespace fw

#endif // IOPROC_HH

