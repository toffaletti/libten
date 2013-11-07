#include "ten/task/rendez.hh"
#include "scheduler.hh"
#include <mutex>

namespace ten {

void rendez::sleep(std::unique_lock<qutex> &lk) {
    DCHECK(lk.owns_lock()) << "must own lock before calling rendez::sleep";
    const auto t = scheduler::current_task();

    {
        std::lock_guard<std::mutex> ll(_m);
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
            task::impl::cancellation_point cancellable;
            t->swap();
        }
        lk.lock();
    } catch (...) {
        ptr<task::impl> wake_task = nullptr;
        {
            std::lock_guard<std::mutex> ll(_m);
            auto i = std::find(_waiting.begin(), _waiting.end(), t);
            if (i != _waiting.end()) {
                _waiting.erase(i);
            } else if (!_waiting.empty()) {
                // this handles the case where the task was woken up
                // by the rendez *and* canceled or deadlined.
                // so it needs to wake up another task instead of itself
                // before it dies otherwise we could deadlock
                wake_task = _waiting.front();
                _waiting.pop_front();
            }
        }
        if (wake_task) {
            DVLOG(5) << "RENDEZ[" << this << "] " << scheduler::current_task() << " wakeup instead: " << wake_task;
            wake_task->ready();
        }
        throw;
    }
}

void rendez::wakeup() {
    ptr<task::impl> t = nullptr;
    {
        std::lock_guard<std::mutex> lk(_m);
        if (!_waiting.empty()) {
            t = _waiting.front();
            _waiting.pop_front();
        }
    }

    DVLOG(5) << "RENDEZ[" << this << "] " << scheduler::current_task() << " wakeup: " << t;
    if (t) t->ready();
}

void rendez::wakeupall() {
    std::deque<ptr<task::impl>> waiting;
    {
        std::lock_guard<std::mutex> lk(_m);
        std::swap(waiting, _waiting);
    }

    for (auto t : waiting) {
        DVLOG(5) << "RENDEZ[" << this << "] " << scheduler::current_task() << " wakeupall: " << t;
        t->ready();
    }
}

rendez::~rendez() {
    using ::operator<<;
    std::lock_guard<std::mutex> lk(_m);
    DCHECK(_waiting.empty()) << "BUG: still waiting: " << _waiting;
}

} // namespace
