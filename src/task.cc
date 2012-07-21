#include <cassert>
#include <algorithm>
#include "proc.hh"
#include "io.hh"

namespace ten {

static std::atomic<uint64_t> taskidgen(0);

void tasksleep(uint64_t ms) {
    this_proc()->sched().sleep(milliseconds(ms));
}

bool fdwait(int fd, int rw, uint64_t ms) {
    return this_proc()->sched().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, uint64_t ms) {
    return this_proc()->sched().poll(fds, nfds, ms);
}

uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    task *t = this_proc()->newtaskinproc(f, stacksize);
    t->ready();
    return t->id;
}

uint64_t taskid() {
    DCHECK(this_proc());
    DCHECK(this_proc()->ctask);
    return this_proc()->ctask->id;
}

int64_t taskyield() {
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
    if (!p->ctask->systask) {
        p->ctask->systask = true;
        --p->taskcount;
    }
}

bool taskcancel(uint64_t id) {
    proc *p = this_proc();
    task *t = nullptr;
    for (auto i = p->alltasks.cbegin(); i != p->alltasks.cend(); ++i) {
        if ((*i)->id == id) {
            t = *i;
            break;
        }
    }

    if (t) {
        t->cancel();
    }
    return (bool)t;
}

const char *taskname(const char *fmt, ...)
{
    task *t = this_proc()->ctask;
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
	task *t = this_proc()->ctask;
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetstate(fmt, arg);
        va_end(arg);
    }
    return t->state;
}

string taskdump() {
    std::stringstream ss;
    proc *p = this_proc();
    DCHECK(p) << "BUG: taskdump called in null proc";
    task *t = nullptr;
    for (auto i = p->alltasks.cbegin(); i != p->alltasks.cend(); ++i) {
        t = *i;
        ss << t << "\n";
    }
    return ss.str();
}

void taskdumpf(FILE *of) {
    string dump = taskdump();
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
    if (exiting) return;
    proc *p = cproc;
    if (!_ready.exchange(true)) {
        DVLOG(5) << "task readying: " << this;
        if (p != this_proc()) {
            p->dirtyq.push(this);
            p->wakeup();
        } else {
            p->runqueue.push_back(this);
        }
    }
}


task::~task() {
    clear(false);
}

void task::clear(bool newid) {
    fn = nullptr;
    _ready = false;
    exiting = false;
    systask = false;
    canceled = false;
    unwinding = false;
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

void task::swap() {
    // swap to scheduler coroutine
    co.swap(&this_proc()->co);

    if (canceled && !unwinding) {
        unwinding = true;
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
                std::rethrow_exception(tmp->exception);
            }
        } else {
            break;
        }
    }
}

deadline::deadline(milliseconds ms) {
    task *t = this_proc()->ctask;
    timeout_id = this_proc()->sched().add_timeout(t, ms, deadline_reached());
}

void deadline::cancel() {
    if (timeout_id) {
        task *t = this_proc()->ctask;
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

} // end namespace ten
