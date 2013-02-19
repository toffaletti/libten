#include <cassert>
#include <algorithm>
#include "proc.hh"
#include "io.hh"

namespace ten {

static std::atomic<uint64_t> taskidgen(0);

std::ostream &operator << (std::ostream &o, ptr<task::pimpl> t) {
    if (t) {
        o << "[" << (void*)t.get() << " " << t->id << " "
            << t->name.get() << " |" << t->state.get()
            << "| sys: " << t->systask
            << " canceled: " << t->canceled << "]";
    } else {
        o << "nulltask";
    }
    return o;
}

void tasksleep(uint64_t ms) {
    task::pimpl::cancellation_point cancellable;
    this_proc()->sched().sleep(milliseconds{ms});
}

bool fdwait(int fd, int rw, optional_timeout ms) {
    task::pimpl::cancellation_point cancellable;
    return this_proc()->sched().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
    task::pimpl::cancellation_point cancellable;
    return this_proc()->sched().poll(fds, nfds, ms);
}

uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    ptr<task::pimpl> t = this_proc()->newtaskinproc(f, stacksize);
    t->ready(true); // add new tasks to front of runqueue
    return t->id;
}

uint64_t taskid() {
    DCHECK(this_proc());
    DCHECK(this_task());
    return this_task()->id;
}

void taskyield() {
    task::pimpl::cancellation_point cancellable;
    ptr<task::pimpl> t = this_task();
    t->ready();
    taskstate("yield");
    t->swap();
}

void tasksystem() {
    ptr<proc> p = this_proc();
    p->mark_system_task();
}

bool taskcancel(uint64_t id) {
    ptr<proc> p = this_proc();
    DCHECK(p) << "BUG: taskcancel called in null proc";
    return p->cancel_task_by_id(id);
}

const char *taskname(const char *fmt, ...)
{
    ptr<task::pimpl> t = this_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetname(fmt, arg);
        va_end(arg);
    }
    return t->name.get();
}

const char *taskstate(const char *fmt, ...)
{
	ptr<task::pimpl> t = this_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetstate(fmt, arg);
        va_end(arg);
    }
    return t->state.get();
}

std::string taskdump() {
    std::stringstream ss;
    ptr<proc> p = this_proc();
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
    : _pimpl{std::make_shared<pimpl>(f, stacksize)}
{
}

uint64_t task::id() const {
    return _pimpl->id;
}

void task::pimpl::init(const std::function<void ()> &f) {
    fn = f;
    ctx.init(task::pimpl::start, ctx.stack_size());
}

void task::pimpl::start(intptr_t arg) {
    task::pimpl *t = reinterpret_cast<task::pimpl *>(arg);
    try {
        if (!t->canceled) {
            t->fn();
        }
    } catch (task_interrupted &e) {
        DVLOG(5) << t << " interrupted ";
    } catch (backtrace_exception &e) {
        LOG(ERROR) << "unhandled error in " << t << ": " << e.what() << "\n" << e.backtrace_str();
        std::exit(2);
    } catch (std::exception &e) {
        LOG(ERROR) << "unhandled error in " << t << ": " << e.what();
        std::exit(2);
    }
    t->exit();
}

void task::pimpl::ready(bool front) {
    ptr<proc> p = cproc;
    if (!is_ready.exchange(true)) {
        p->ready(ptr<task::pimpl>{this}, front);
    }
}

void task::pimpl::setname(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetname(fmt, arg);
    va_end(arg);
}

void task::pimpl::vsetname(const char *fmt, va_list arg) {
    vsnprintf(name.get(), namesize, fmt, arg);
}

void task::pimpl::setstate(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetstate(fmt, arg);
    va_end(arg);
}

void task::pimpl::vsetstate(const char *fmt, va_list arg) {
    vsnprintf(state.get(), statesize, fmt, arg);
}

void task::pimpl::clear(bool newid) {
    fn = nullptr;
    cancel_points = 0;
    is_ready = false;
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

void task::pimpl::safe_swap() noexcept {
    // swap to scheduler coroutine
    ctx.swap(this_proc()->sched_context(), 0);
}

void task::pimpl::swap() {
    // swap to scheduler coroutine
    ctx.swap(this_proc()->sched_context(), 0);

    if (canceled && cancel_points > 0) {
        DVLOG(5) << "THROW INTERRUPT: " << this << "\n" << saved_backtrace().str();
        throw task_interrupted();
    }

    if (exception && cancel_points > 0) {
        DVLOG(5) << "THROW TIMEOUT: " << this << "\n" << saved_backtrace().str();
        std::exception_ptr tmp = nullptr;
        std::swap(tmp, exception);
        std::rethrow_exception(tmp);
    }
}

void task::pimpl::exit() {
    fn = nullptr;
    swap();
}

bool task::pimpl::cancelable() const {
    return cancel_points > 0;
}

void task::pimpl::cancel() {
    // don't cancel systasks
    if (systask) return;
    canceled = true;
    ready();
}

task::pimpl::~pimpl() {
    clear(false);
}

struct deadline_pimpl {
    io_scheduler::alarm_clock::scoped_alarm alarm;
};

deadline::deadline(optional_timeout timeout) {
    if (timeout) {
        if (timeout->count() == 0)
            throw errorx("zero optional_deadline - misuse of optional");
        _set_deadline(*timeout);
    }
}

deadline::deadline(milliseconds ms) {
    _set_deadline(ms);
}

void deadline::_set_deadline(milliseconds ms) {
    if (ms.count() < 0)
        throw errorx("negative deadline: %jdms", intmax_t(ms.count()));
    if (ms.count() > 0) {
        ptr<proc> p = this_proc();
        ptr<task::pimpl> t = this_task();
        auto now = p->cached_time();
        _pimpl.reset(new deadline_pimpl{});
        _pimpl->alarm = std::move(io_scheduler::alarm_clock::scoped_alarm{
                p->sched().alarms, t, ms+now, deadline_reached{}
                });
        DVLOG(5) << "deadline alarm armed: " << _pimpl->alarm._armed << " in " << ms.count() << "ms";
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

task::pimpl::cancellation_point::cancellation_point() {
    ptr<task::pimpl> t = this_task();
    ++t->cancel_points;
}

task::pimpl::cancellation_point::~cancellation_point() {
    ptr<task::pimpl> t = this_task();
    --t->cancel_points;
}


} // end namespace ten
