#ifndef LIBTEN_IOPROC_HH
#define LIBTEN_IOPROC_HH

#include "ten/thread_guard.hh"
#include "ten/task.hh"
#include "ten/channel.hh"
#include <boost/any.hpp>
#include <type_traits>
#include <exception>
#include <memory>
#include <fcntl.h>

namespace ten {

// anyfunc - hold and call a function that may return void non-void

using anyfunc = std::function<boost::any ()>;

namespace anyfunc_impl {
template <typename F> anyfunc make(F f,   std::true_type)   { return [f]() mutable -> boost::any { f(); return {}; }; }
template <typename F> anyfunc make(F &&f, std::false_type)  { return anyfunc(std::forward<F>(f)); }
}

template <typename F, typename R = typename std::result_of<F()>::type>
inline anyfunc make_anyfunc(F f) {
    return anyfunc_impl::make(std::move(f), typename std::is_void<R>::type());
}

inline anyfunc make_anyfunc(const anyfunc  &f) { return f; }
inline anyfunc make_anyfunc(      anyfunc &&f) { return std::move(f); }

// return a boost::any even if it's empty

template <class T> inline T    any_value(      const boost::any  &a) { return boost::any_cast<T>(a); }
template <class T> inline T    any_value(            boost::any &&a) { return std::move(boost::any_cast<T &>(a)); }
template <>        inline void any_value<void>(const boost::any  &)  {}
template <>        inline void any_value<void>(      boost::any &&)  {}

// thread pool for io or other tasks

struct pcall;
using iochannel = channel<std::unique_ptr<pcall>>;

//! remote call in another thread
struct pcall {
    iochannel ch;
    anyfunc op;
    std::exception_ptr exception;
    boost::any ret;

    pcall(anyfunc op_, iochannel &ch_) : ch(ch_), op(std::move(op_)) {}
};

void ioproctask(iochannel &);

//! a pool of threads for making blocking calls
struct ioproc {
    iochannel ch;
    std::vector<thread_guard> threads;

    ioproc(size_t stacksize = 0/*deprecated*/,
           unsigned nprocs = 1,
           unsigned chanbuf = 0,
           std::function<void(iochannel &)> proctask = ioproctask)
        : ch(chanbuf ? chanbuf : nprocs)
    {
        // TODO: stacksize no longer used
        for (unsigned i=0; i<nprocs; ++i) {
            threads.emplace_back(task::spawn_thread([=] {
                proctask(ch);
            }));
        }
    }

    ~ioproc() {
        DVLOG(5) << "closing ioproc channel: " << this;
        ch.close();
        DVLOG(5) << "freeing ioproc: " << this;
    }
};

namespace ioproc_impl {
}

//! wait on an iochannel for a call to complete with a result
template <typename ResultT>
ResultT iowait(iochannel &reply_chan) {
    try {
        std::unique_ptr<pcall> reply(reply_chan.recv());
        if (reply->exception != nullptr) {
            std::rethrow_exception(reply->exception);
        }
        return any_value<ResultT>(std::move(reply->ret));
    } catch (task_interrupted &e) {
        reply_chan.close();
        throw;
    }
}

//! make an iocall, but dont wait for it to complete
template <typename Func>
void iocallasync(ioproc &io, Func &&f, iochannel reply_chan = iochannel()) {
    std::unique_ptr<pcall> call(new pcall(make_anyfunc(f), reply_chan));
    io.ch.send(std::move(call));
}

//! make an iocall, and wait for the result
template <typename Func, typename Result = typename std::result_of<Func()>::type>
Result iocall(ioproc &io, Func &&f, iochannel reply_chan = iochannel()) {
    std::unique_ptr<pcall> call(new pcall(make_anyfunc(f), reply_chan));
    io.ch.send(std::move(call));
    return iowait<Result>(reply_chan);
}

////// iorw /////

template <typename ProcT> int ioopen(ProcT &io, char *path, int mode) {
    return iocall(io, std::bind((int (*)(char *, int))::open, path, mode));
}

template <typename ProcT>  int ioclose(ProcT &io, int fd) {
    return iocall(io, std::bind(::close, fd));
}

template <typename ProcT> ssize_t ioread(ProcT &io, int fd, void *buf, size_t n) {
    return iocall(io, std::bind(::read, fd, buf, n));
}

template <typename ProcT> ssize_t iowrite(ProcT &io, int fd, void *buf, size_t n) {
    return iocall(io, std::bind(::write, fd, buf, n));
}

} // end namespace ten 

#endif // LIBTEN_IOPROC_HH
