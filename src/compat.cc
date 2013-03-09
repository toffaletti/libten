#include "thread_context.hh"

namespace ten {

void tasksleep(uint64_t ms) {
    this_task::sleep_for(std::chrono::milliseconds{ms});
}

bool fdwait(int fd, int rw, optional_timeout ms) {
    task::impl::cancellation_point cancellable;
    return this_ctx->scheduler.get_io().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
    task::impl::cancellation_point cancellable;
    return this_ctx->scheduler.get_io().poll(fds, nfds, ms);
}

uint64_t taskid() {
    return this_task::get_id();
}

void taskyield() {
    this_task::yield();
}

bool taskcancel(uint64_t id) {
    return this_ctx->scheduler.cancel_task_by_id(id);
}

const char *taskname(const char *fmt, ...)
{
    const auto t = kernel::current_task();
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
    const auto t = kernel::current_task();
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
