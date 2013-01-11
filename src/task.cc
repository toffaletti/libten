#include <cassert>
#include <algorithm>
#include "proc.hh"
#include "io.hh"

namespace ten {

static std::atomic<uint64_t> taskidgen(0);

void tasksleep(uint64_t ms) {
    task::cancellation_point cancellable;
    this_proc()->sched().sleep(milliseconds(ms));
}

bool fdwait(int fd, int rw, optional_timeout ms) {
    task::cancellation_point cancellable;
    return this_proc()->sched().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
    task::cancellation_point cancellable;
    return this_proc()->sched().poll(fds, nfds, ms);
}

uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    task *t = this_proc()->newtaskinproc(f, stacksize);
    t->ready(true); // add new tasks to front of runqueue
    return t->id;
}

uint64_t taskid() {
    DCHECK(this_proc());
    DCHECK(this_task());
    return this_task()->id;
}

int64_t taskyield() {
    task::cancellation_point cancellable;
    proc *p = this_proc();
    uint64_t n = p->nswitch;
    task *t = p->ctask;
    t->ready();
    taskstate("yield");
    t->swap();
    DVLOG(5) << "yield: " << (int64_t)(p->nswitch - n - 1);
    return p->nswitch - n - 1;
}

void tasksystem() {
    proc *p = this_proc();
    p->mark_system_task();
}

bool taskcancel(uint64_t id) {
    proc *p = this_proc();
    DCHECK(p) << "BUG: taskcancel called in null proc";
    return p->cancel_task_by_id(id);
}

const char *taskname(const char *fmt, ...)
{
    task *t = this_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetname(fmt, arg);
        va_end(arg);
    }
    return t->name;
}

const char *taskstate(const char *fmt, ...)
{
	task *t = this_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetstate(fmt, arg);
        va_end(arg);
    }
    return t->state;
}

std::string taskdump() {
    std::stringstream ss;
    proc *p = this_proc();
    DCHECK(p) << "BUG: taskdump called in null proc";
    p->dump_tasks(ss);
    return ss.str();
}

void taskdumpf(FILE *of) {
    std::string dump = taskdump();
    fwrite(dump.c_str(), sizeof(char), dump.size(), of);
    fflush(of);
}

task::task(const std::function<void ()> &f, size_t stacksize)
    : co(task::start, this, stacksize)
{
    clear();
    fn = f;
}

void task::init(const std::function<void ()> &f) {
    fn = f;
    co.restart(task::start, this);
}

void task::ready(bool front) {
    proc *p = cproc;
    if (!_ready.exchange(true)) {
        p->ready(this, front);
    }
}


task::~task() {
    clear(false);
}

void task::clear(bool newid) {
    fn = nullptr;
    cancel_points = 0;
    _ready = false;
    systask = false;
    canceled = false;
    if (newid) {
        id = ++taskidgen;
        setname("task[%ju]", id);
        setstate("new");
    }
    exception = nullptr;
    cproc = nullptr;
}

void task::safe_swap() noexcept {
    // swap to scheduler coroutine
    co.swap(&this_proc()->sched_coro());
}

void task::swap() {
    // swap to scheduler coroutine
    co.swap(&this_proc()->sched_coro());

    if (canceled && cancel_points > 0) {
        DVLOG(5) << "THROW INTERRUPT: " << this << "\n" << saved_backtrace().str();
        throw task_interrupted();
    }

    if (exception) {
        DVLOG(5) << "THROW TIMEOUT: " << this << "\n" << saved_backtrace().str();
        std::exception_ptr tmp = nullptr;
        std::swap(tmp, exception);
        std::rethrow_exception(tmp);
    }
}

deadline::deadline(optional_timeout timeout) {
    if (timeout) {
        if (timeout->count() == 0)
            throw errorx("zero optional_deadline - misuse of optional");
        _set_deadline(*timeout);
    }
}

struct deadline_pimpl {
    io_scheduler::alarm_clock::scoped_alarm alarm;
};

deadline::deadline(milliseconds ms) {
    _set_deadline(ms);
}

void deadline::_set_deadline(milliseconds ms) {
    if (ms.count() < 0)
        throw errorx("negative deadline: %jdms", intmax_t(ms.count()));
    if (ms.count() > 0) {
        proc *p = this_proc();
        task *t = this_task();
        auto now = p->cached_time();
        _pimpl.reset(new deadline_pimpl{});
        _pimpl->alarm = std::move(io_scheduler::alarm_clock::scoped_alarm{
                p->sched().alarms, t, ms+now, deadline_reached{}
                });
    }
}

void deadline::cancel() {
    if (_pimpl) {
        _pimpl->alarm.cancel();
    }
}

deadline::~deadline() {
    cancel();
}

milliseconds deadline::remaining() const {
    if (_pimpl) {
        return _pimpl->alarm.remaining();
    }
    return {};
}

task::cancellation_point::cancellation_point() {
    task *t = this_task();
    ++t->cancel_points;
}

task::cancellation_point::~cancellation_point() {
    task *t = this_task();
    --t->cancel_points;
}


} // end namespace ten
