#include <condition_variable>
#include <cassert>
#include <algorithm>
#include "task/private.hh"
#include "task/proc.hh"
#include "task/io.hh"

namespace ten {

using std::function;
using std::atomic;
using std::stringstream;
using std::mutex;
using std::timed_mutex;
using std::unique_lock;
using std::unique_ptr;
using std::rethrow_exception;
using std::try_to_lock;

static atomic<uint64_t> taskidgen(0);

void tasksleep(uint64_t ms) {
    _this_proc->sched().sleep(milliseconds(ms));
}

bool fdwait(int fd, int rw, uint64_t ms) {
    return _this_proc->sched().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, uint64_t ms) {
    return _this_proc->sched().poll(fds, nfds, ms);
}

uint64_t taskspawn(const function<void ()> &f, size_t stacksize) {
    task *t = _this_proc->newtaskinproc(f, stacksize);
    t->ready();
    return t->id;
}

uint64_t taskid() {
    CHECK(_this_proc);
    CHECK(_this_proc->ctask);
    return _this_proc->ctask->id;
}

int64_t taskyield() {
    proc *p = _this_proc;
    uint64_t n = p->nswitch;
    task *t = p->ctask;
    t->ready();
    taskstate("yield");
    t->swap();
    DVLOG(5) << "yield: " << (int64_t)(p->nswitch - n - 1);
    return p->nswitch - n - 1;
}

void tasksystem() {
    proc *p = _this_proc;
    if (!p->ctask->systask) {
        p->ctask->systask = true;
        --p->taskcount;
    }
}

bool taskcancel(uint64_t id) {
    proc *p = _this_proc;
    task *t = 0;
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
    task *t = _this_proc->ctask;
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
	task *t = _this_proc->ctask;
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetstate(fmt, arg);
        va_end(arg);
    }
    return t->state;
}

string taskdump() {
    stringstream ss;
    proc *p = _this_proc;
    CHECK(p) << "BUG: taskdump called in null proc";
    task *t = 0;
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

task::task(const function<void ()> &f, size_t stacksize)
    : co(task::start, this, stacksize)
{
    clear();
    fn = f;
}

void task::init(const function<void ()> &f) {
    fn = f;
    co.restart(task::start, this);
}

void task::ready() {
    if (exiting) return;
    proc *p = cproc;
    unique_lock<mutex> lk(p->mutex);
    if (find(p->runqueue.cbegin(), p->runqueue.cend(), this) == p->runqueue.cend()) {
        DVLOG(5) << _this_proc->ctask << " adding task: " << this << " to runqueue for proc: " << p;
        p->runqueue.push_back(this);
    } else {
        DVLOG(5) << "found task: " << this << " already in runqueue for proc: " << p;
    }
    // XXX: does this need to be outside of the if(!found) ?
    if (p != _this_proc) {
        p->wakeupandunlock(lk);
    }
}


task::~task() {
    clear(false);
}

void task::clear(bool newid) {
    fn = 0;
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

    cproc = 0;
}

void task::remove_timeout(timeout_t *to) {
    auto i = find(timeouts.begin(), timeouts.end(), to);
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
    co.swap(&_this_proc->co);

    if (canceled && !unwinding) {
        unwinding = true;
        DVLOG(5) << "THROW INTERRUPT: " << this << "\n" << saved_backtrace().str();
        throw task_interrupted();
    }

    while (!timeouts.empty()) {
        timeout_t *to = timeouts.front();
        if (to->when <= procnow()) {
            unique_ptr<timeout_t> tmp(to); // ensure to is freed
            DVLOG(5) << to << " reached for " << this << " removing.";
            timeouts.pop_front();
            if (timeouts.empty()) {
                // remove from scheduler timeout list
                cproc->sched().remove_timeout_task(this);
            }
            if (tmp->exception != 0) {
                rethrow_exception(tmp->exception);
            }
        } else {
            break;
        }
    }
}

void qutex::lock() {
    task *t = _this_proc->ctask;
    CHECK(t) << "BUG: qutex::lock called outside of task";
    {
        unique_lock<timed_mutex> lk(m);
        if (owner == 0 || owner == t) {
            owner = t;
            DVLOG(5) << "LOCK qutex: " << this << " owner: " << owner;
            return;
        }
        DVLOG(5) << "LOCK waiting: " << this << " add: " << t <<  " owner: " << owner;
        waiting.push_back(t);
    }

    try {
        // loop to handle spurious wakeups from other threads
        for (;;) {
            t->swap();
            unique_lock<timed_mutex> lk(m);
            if (owner == _this_proc->ctask) {
                break;
            }
        }
    } catch (...) {
        unique_lock<timed_mutex> lk(m);
        internal_unlock(lk);
        throw;
    }
}

bool qutex::try_lock() {
    task *t = _this_proc->ctask;
    CHECK(t) << "BUG: qutex::try_lock called outside of task";
    unique_lock<timed_mutex> lk(m, try_to_lock);
    if (lk.owns_lock()) {
        if (owner == 0) {
            owner = t;
            return true;
        }
    }
    return false;
}

void qutex::unlock() {
    unique_lock<timed_mutex> lk(m);
    internal_unlock(lk);
}

void qutex::internal_unlock(unique_lock<timed_mutex> &lk) {
    task *t = _this_proc->ctask;
    CHECK(lk.owns_lock()) << "BUG: lock not owned " << t;
    if (t == owner) {
        if (!waiting.empty()) {
            t = owner = waiting.front();
            waiting.pop_front();
        } else {
            t = owner = 0;
        }
        DVLOG(5) << "UNLOCK qutex: " << this << " new owner: " << owner << " waiting: " << waiting.size();
        lk.unlock();
        // must use t here, not owner because
        // lock has been released
        if (t) t->ready();
    } else {
        // this branch is taken when exception is thrown inside
        // a task that is currently waiting inside qutex::lock
        auto i = find(waiting.begin(), waiting.end(), t);
        if (i != waiting.end()) {
            waiting.erase(i);
        }
    }
}

#if 0
bool rendez::sleep_for(unique_lock<qutex> &lk, unsigned int ms) {
    task *t = _this_proc->ctask;
    if (find(waiting.begin(), waiting.end(), t) == waiting.end()) {
        DVLOG(5) << "RENDEZ SLEEP PUSH BACK: " << t;
        waiting.push_back(t);
    }
    lk.unlock();
    _this_proc->sched().add_timeout(t, ms);
    t->swap();
    lk.lock();
    _this_proc->sched().del_timeout(t);
    // if we're not in the waiting list then we were signaled to wakeup
    return find(waiting.begin(), waiting.end(), t) == waiting.end();
}
#endif

void rendez::sleep(unique_lock<qutex> &lk) {
    task *t = _this_proc->ctask;

    {
        unique_lock<timed_mutex> ll(m);
        CHECK(find(waiting.begin(), waiting.end(), t) == waiting.end())
            << "BUG: " << t << " already waiting on rendez " << this;
        DVLOG(5) << "RENDEZ " << this << " PUSH BACK: " << t;
        waiting.push_back(t);
    }
    // must hold the lock until we're in the waiting list
    // otherwise another thread might modify the condition and
    // call wakeup() and waiting would be empty so we'd sleep forever
    lk.unlock();
    try {
        t->swap(); 
        lk.lock();
    } catch (...) {
        unique_lock<timed_mutex> ll(m);
        auto i = find(waiting.begin(), waiting.end(), t);
        if (i != waiting.end()) {
            waiting.erase(i);
        }
        lk.lock();
        throw;
    }
}

void rendez::wakeup() {
    unique_lock<timed_mutex> lk(m);
    if (!waiting.empty()) {
        task *t = waiting.front();
        waiting.pop_front();
        DVLOG(5) << "RENDEZ " << this << " wakeup: " << t;
        t->ready();
    }
}

void rendez::wakeupall() {
    unique_lock<timed_mutex> lk(m);
    while (!waiting.empty()) {
        task *t = waiting.front();
        waiting.pop_front();
        DVLOG(5) << "RENDEZ " << this << " wakeupall: " << t;
        t->ready();
    }
}


rendez::~rendez() {
    using ::operator<<;
    unique_lock<timed_mutex> lk(m);
    CHECK(waiting.empty()) << "BUG: still waiting: " << waiting;
}

deadline::deadline(uint64_t ms) {
    task *t = _this_proc->ctask;
    timeout_id = _this_proc->sched().add_timeout(t, milliseconds(ms), deadline_reached());
}

deadline::~deadline() {
    task *t = _this_proc->ctask;
    t->remove_timeout((task::timeout_t *)timeout_id);
}

} // end namespace ten
