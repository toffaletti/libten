#include "ten/task/rendez.hh"
#include "proc.hh"
#include <mutex>

namespace ten {

void rendez::sleep(std::unique_lock<qutex> &lk) {
    DCHECK(lk.owns_lock()) << "must own lock before calling rendez::sleep";
    task *t = this_task();

    {
        std::lock_guard<std::timed_mutex> ll(_m);
        DCHECK(std::find(_waiting.begin(), _waiting.end(), t) == _waiting.end())
            << "BUG: " << t << " already waiting on rendez " << this;
        DVLOG(5) << "RENDEZ[" << this << "] PUSH BACK: " << t;
        _waiting.push_back(t);
    }
    // must hold the lock until we're in the waiting list
    // otherwise another thread might modify the condition and
    // call wakeup() and waiting would be empty so we'd sleep forever
    lk.unlock();
    try 
    {
        {
            task::cancellation_point cancellable;
            t->swap(); 
        }
        lk.lock();
    } catch (...) {
        {
            std::lock_guard<std::timed_mutex> ll(_m);
            auto i = std::find(_waiting.begin(), _waiting.end(), t);
            if (i != _waiting.end()) {
                _waiting.erase(i);
            }
        }
        lk.lock();
        throw;
    }
}

void rendez::wakeup() {
    task *t = nullptr;
    {
        std::lock_guard<std::timed_mutex> lk(_m);
        if (!_waiting.empty()) {
            t = _waiting.front();
            _waiting.pop_front();
        }
    }

    DVLOG(5) << "RENDEZ[" << this << "] " << this_task() << " wakeup: " << t;
    if (t) t->ready();
}

void rendez::wakeupall() {
    tasklist waiting;
    {
        std::lock_guard<std::timed_mutex> lk(_m);
        std::swap(waiting, _waiting);
    }

    for (auto t : waiting) {
        DVLOG(5) << "RENDEZ[" << this << "] " << this_task() << " wakeupall: " << t;
        t->ready();
    }
}

#if 0
bool rendez::sleep_for(unique_lock<qutex> &lk, unsigned int ms) {
    task *t = this_proc()->ctask;
    if (find(waiting.begin(), waiting.end(), t) == waiting.end()) {
        DVLOG(5) << "RENDEZ SLEEP PUSH BACK: " << t;
        waiting.push_back(t);
    }
    lk.unlock();
    this_proc()->sched().add_timeout(t, ms);
    t->swap();
    lk.lock();
    this_proc()->sched().del_timeout(t);
    // if we're not in the waiting list then we were signaled to wakeup
    return find(waiting.begin(), waiting.end(), t) == waiting.end();
}
#endif

rendez::~rendez() {
    using ::operator<<;
    std::lock_guard<std::timed_mutex> lk(_m);
    DCHECK(_waiting.empty()) << "BUG: still waiting: " << _waiting;
}

} // namespace
