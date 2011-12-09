#ifndef IOPROC_HH
#define IOPROC_HH

#include "task.hh"
#include "channel.hh"
#include <stdarg.h>
#include <memory>
#include <boost/any.hpp>
#include "shared_pool.hh"
#include <fcntl.h>
#include <cassert>

namespace ten {

struct pcall;
typedef channel<std::unique_ptr<pcall> > iochannel;

struct pcall {
    iochannel ch;
    std::function<void ()> vop;
    std::function<boost::any ()> op;
    char err[256];
    boost::any ret;

    pcall(const std::function<void ()> &vop_, iochannel &ch_) : ch(ch_), vop(vop_) {}
    pcall(const std::function<boost::any ()> &op_, iochannel &ch_) : ch(ch_), op(op_) {}
};

void ioproctask(iochannel &);

struct ioproc {
    iochannel ch;
    std::vector<uint64_t> tids;

    ioproc(
            size_t stacksize = default_stacksize,
            unsigned nprocs = 1,
            unsigned chanbuf = 0,
            void (*proctask)(iochannel &) = ioproctask)
        : ch(chanbuf ? chanbuf : nprocs)
    {
        for (unsigned i=0; i<nprocs; ++i) {
            tids.push_back(procspawn(std::bind(proctask, ch), stacksize));
        }
    }

    ~ioproc() {
        DVLOG(5) << "closing ioproc channel: " << this;
        ch.close();
        DVLOG(5) << "freeing ioproc: " << this;
    }
};

inline void iowait(iochannel &reply_chan) {
    // TODO: maybe this close logic needs to be in channel itself?
    // is there ever a case where you'd want a channel to stay open
    // after a task using it has been interrupted? i can't think of one
    try {
        std::unique_ptr<pcall> reply(reply_chan.recv());
    } catch (task_interrupted &e) {
        reply_chan.close();
        throw;
    }
}

template <typename ReturnT>
ReturnT iowait(iochannel &reply_chan) {
    try {
        std::unique_ptr<pcall> reply(reply_chan.recv());
        return boost::any_cast<ReturnT>(reply->ret);
    } catch (task_interrupted &e) {
        reply_chan.close();
        throw;
    }
}

template <typename ReturnT>
void iocallasync(
        ioproc &io,
        const std::function<boost::any ()> &op,
        iochannel reply_chan = iochannel())
{
    std::unique_ptr<pcall> call(new pcall(op, reply_chan));
    io.ch.send(std::move(call));
}

inline void iocallasync(
        ioproc &io,
        const std::function<void ()> &vop,
        iochannel reply_chan = iochannel())
{
    std::unique_ptr<pcall> call(new pcall(vop, reply_chan));
    io.ch.send(std::move(call));
}

template <typename ReturnT>
ReturnT iocall(
        ioproc &io,
        const std::function<boost::any ()> &op,
        iochannel reply_chan = iochannel())
{
    std::unique_ptr<pcall> call(new pcall(op, reply_chan));
    io.ch.send(std::move(call));
    return iowait<ReturnT>(reply_chan);
}

inline void iocall(
        ioproc &io,
        const std::function<void ()> &vop,
        iochannel reply_chan = iochannel())
{
    std::unique_ptr<pcall> call(new pcall(vop, reply_chan));
    io.ch.send(std::move(call));
    iowait(reply_chan);
}

////// iorw /////

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

extern int netdial(int fd, const char *addr, uint16_t port);

template <typename ProcT> int iodial(ProcT &io, int fd, const char *addr, uint64_t port) {
    return iocall<int>(io, std::bind(netdial, fd, addr, port));
}

} // end namespace ten 

#endif // IOPROC_HH

