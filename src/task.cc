#include <cassert>
#include <algorithm>
#include "proc.hh"
#include "io.hh"

namespace ten {

static std::atomic<uint64_t> taskidgen(0);

std::ostream &operator << (std::ostream &o, ptr<task> t) {
    if (t && t->_pimpl) {
        task::pimpl *p = t->_pimpl.get();
        o << "[" << (void*)p << " " << p->id << " "
            << p->name << " |" << p->state
            << "| sys: " << p->systask
            << " canceled: " << p->canceled << "]";
    } else {
        o << "nulltask";
    }
    return o;
}

void tasksleep(uint64_t ms) {
    task::cancellation_point cancellable;
    this_proc()->sched().sleep(milliseconds{ms});
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
    ptr<task> t = this_proc()->newtaskinproc(f, stacksize);
    t->ready(true); // add new tasks to front of runqueue
    return t->_pimpl->id;
}

uint64_t taskid() {
    DCHECK(this_proc());
    DCHECK(this_task());
    return this_task()->id();
}

void taskyield() {
    task::cancellation_point cancellable;
    ptr<task> t = this_task();
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
    ptr<task> t = this_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->_pimpl->vsetname(fmt, arg);
        va_end(arg);
    }
    return t->_pimpl->name;
}

const char *taskstate(const char *fmt, ...)
{
	ptr<task> t = this_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->_pimpl->vsetstate(fmt, arg);
        va_end(arg);
    }
    return t->_pimpl->state;
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

void task::init(const std::function<void ()> &f) {
    _pimpl->fn = f;
    _pimpl->ctx.init(task::start, _pimpl->ctx.stack_size());
}

void task::start(intptr_t arg) {
    task *t = reinterpret_cast<task *>(arg);
    try {
        if (!t->_pimpl->canceled) {
            t->_pimpl->fn();
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

void task::ready(bool front) {
    ptr<proc> p = _pimpl->cproc;
    if (!_pimpl->ready.exchange(true)) {
        p->ready(ptr<task>{this}, front);
    }
}

void task::clear(bool newid) {
    _pimpl->clear(newid);
}

uint64_t task::id() const {
    return _pimpl->id;
}

void task::pimpl::setname(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetname(fmt, arg);
    va_end(arg);
}

void task::pimpl::vsetname(const char *fmt, va_list arg) {
    vsnprintf(name, sizeof(name), fmt, arg);
}

void task::pimpl::setstate(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetstate(fmt, arg);
    va_end(arg);
}

void task::pimpl::vsetstate(const char *fmt, va_list arg) {
    vsnprintf(state, sizeof(state), fmt, arg);
}

void task::pimpl::clear(bool newid) {
    fn = nullptr;
    cancel_points = 0;
    ready = false;
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
    _pimpl->ctx.swap(this_proc()->sched_context(), 0);
}

void task::swap() {
    // swap to scheduler coroutine
    _pimpl->ctx.swap(this_proc()->sched_context(), 0);

    if (_pimpl->canceled && _pimpl->cancel_points > 0) {
        DVLOG(5) << "THROW INTERRUPT: " << this << "\n" << saved_backtrace().str();
        throw task_interrupted();
    }

    if (_pimpl->exception && _pimpl->cancel_points > 0) {
        DVLOG(5) << "THROW TIMEOUT: " << this << "\n" << saved_backtrace().str();
        std::exception_ptr tmp = nullptr;
        std::swap(tmp, _pimpl->exception);
        std::rethrow_exception(tmp);
    }
}

void task::exit() {
    _pimpl->fn = nullptr;
    swap();
}

bool task::cancelable() const {
    return _pimpl->cancel_points > 0;
}

void task::cancel() {
    // don't cancel systasks
    if (_pimpl->systask) return;
    _pimpl->canceled = true;
    ready();
}

task::~task() {
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
        ptr<task> t = this_task();
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

task::cancellation_point::cancellation_point() {
    ptr<task> t = this_task();
    ++t->_pimpl->cancel_points;
}

task::cancellation_point::~cancellation_point() {
    ptr<task> t = this_task();
    --t->_pimpl->cancel_points;
}


} // end namespace ten
