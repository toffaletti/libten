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

bool fdwait(int fd, int rw, uint64_t ms) {
    task::cancellation_point cancellable;
    return this_proc()->sched().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, uint64_t ms) {
    task::cancellation_point cancellable;
    return this_proc()->sched().poll(fds, nfds, ms);
}

uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    task *t = this_proc()->newtaskinproc(f, stacksize);
    t->ready();
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

void task::ready() {
    proc *p = cproc;
    if (!_ready.exchange(true)) {
        p->ready(this);
    }
}


task::~task() {
    clear(false);
}

void task::clear(bool newid) {
    fn = nullptr;
    _cancel_points = 0;
    _ready = false;
    systask = false;
    canceled = false;
    if (newid) {
        id = ++taskidgen;
        setname("task[%ju]", id);
        setstate("new");
    }

    if (!timeouts.empty()) {
        // free timeouts
        for (auto i=timeouts.begin(); i<timeouts.end(); ++i) {
            delete *i;
        }
        timeouts.clear();
        // remove from scheduler timeout list
        cproc->sched().remove_timeout_task(this);
    }

    cproc = nullptr;
}

void task::remove_timeout(timeout_t *to) {
    auto i = std::find(timeouts.begin(), timeouts.end(), to);
    if (i != timeouts.end()) {
        delete *i;
        timeouts.erase(i);
    }
    if (timeouts.empty()) {
        // remove from scheduler timeout list
        cproc->sched().remove_timeout_task(this);
    }
}

void task::safe_swap() noexcept {
    // swap to scheduler coroutine
    co.swap(&this_proc()->sched_coro());
}

void task::swap() {
    // swap to scheduler coroutine
    co.swap(&this_proc()->sched_coro());

    if (canceled && _cancel_points > 0) {
        DVLOG(5) << "THROW INTERRUPT: " << this << "\n" << saved_backtrace().str();
        throw task_interrupted();
    }

    while (!timeouts.empty()) {
        timeout_t *to = timeouts.front();
        if (to->when <= procnow()) {
            std::unique_ptr<timeout_t> tmp{to}; // ensure to is freed
            DVLOG(5) << to << " reached for " << this << " removing.";
            timeouts.pop_front();
            if (timeouts.empty()) {
                // remove from scheduler timeout list
                cproc->sched().remove_timeout_task(this);
            }
            if (tmp->exception != nullptr) {
                DVLOG(5) << "THROW TIMEOUT: " << this << "\n" << saved_backtrace().str();
                std::rethrow_exception(tmp->exception);
            }
        } else {
            break;
        }
    }
}

deadline::deadline(milliseconds ms)
  : timeout_id()
{
    if (ms.count() < 0)
        throw errorx("negative deadline: %jdms", intmax_t(ms.count()));
    if (ms.count() > 0) {
        task *t = this_task();
        timeout_id = this_proc()->sched().add_timeout(t, ms, deadline_reached());
    }
}

void deadline::cancel() {
    if (timeout_id) {
        task *t = this_task();
        t->remove_timeout((task::timeout_t *)timeout_id);
        timeout_id = nullptr;
    }
}

deadline::~deadline() {
    cancel();
}

milliseconds deadline::remaining() const {
    task::timeout_t *timeout = (task::timeout_t *)timeout_id;
    // TODO: need a way of distinguishing between canceled and over due
    if (timeout != nullptr) {
        std::chrono::time_point<std::chrono::steady_clock> now = procnow();
        if (now > timeout->when) {
            return milliseconds(0);
        }
        return duration_cast<milliseconds>(timeout->when - now);
    }
    return milliseconds(0);
}

task::cancellation_point::cancellation_point() {
    task *t = this_task();
    ++t->_cancel_points;
}

task::cancellation_point::~cancellation_point() {
    task *t = this_task();
    --t->_cancel_points;
}


} // end namespace ten
