#include "thread_context.hh"

namespace ten {

bool fdwait(int fd, int rw, optional_timeout ms) {
    task::impl::cancellation_point cancellable;
    return this_ctx->scheduler.get_io().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
    task::impl::cancellation_point cancellable;
    return this_ctx->scheduler.get_io().poll(fds, nfds, ms);
}

const char *taskname(const char *fmt, ...)
{
    const auto t = scheduler::current_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetname(fmt, arg);
        va_end(arg);
    }
    return t->getname();
}

const char *taskstate(const char *fmt, ...)
{
    const auto t = scheduler::current_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetstate(fmt, arg);
        va_end(arg);
    }
    return t->getstate();
}

void taskdump() {
    thread_context::dump_all();
}

} // ten
