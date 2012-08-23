#include "ten/task/qutex.hh"
#include "proc.hh"

namespace ten {

void qutex::lock() {
    task *t = this_proc()->ctask;
    DCHECK(t->cancel_points == 0) << "BUG: cannot cancel a lock";
    DCHECK(t) << "BUG: qutex::lock called outside of task";
    {
        std::unique_lock<std::timed_mutex> lk{_m};
        DCHECK(_owner != t) << "no recursive locking";
        if (_owner == nullptr) {
            _owner = t;
            DVLOG(5) << "LOCK qutex: " << this << " owner: " << _owner;
            return;
        }
        DVLOG(5) << "QUTEX[" << this << "] lock waiting add: " << t <<  " owner: " << _owner;
        _waiting.push_back(t);
    }

    // loop to handle spurious wakeups from other threads
    for (;;) {
        DCHECK(t->cancel_points == 0) << "BUG: cannot cancel a lock";
        t->swap();
        std::unique_lock<std::timed_mutex> lk{_m};
        if (_owner == this_proc()->ctask) {
            break;
        }
    }
}

bool qutex::try_lock() {
    task *t = this_proc()->ctask;
    DCHECK(t) << "BUG: qutex::try_lock called outside of task";
    std::unique_lock<std::timed_mutex> lk{_m, std::try_to_lock};
    if (lk.owns_lock()) {
        if (_owner == nullptr) {
            _owner = t;
            return true;
        }
    }
    return false;
}

void qutex::unlock() {
    std::unique_lock<std::timed_mutex> lk{_m};
    task *t = this_proc()->ctask;
    DCHECK(lk.owns_lock()) << "BUG: lock not owned " << t;
    DVLOG(5) << "QUTEX[" << this << "] unlock: " << t;
    DCHECK(t == _owner) << "BUG: lock not owned by this task";
    if (!_waiting.empty()) {
        t = _owner = _waiting.front();
        _waiting.pop_front();
    } else {
        t = _owner = nullptr;
    }
    DVLOG(5) << "UNLOCK qutex: " << this
        << " new owner: " << _owner
        << " waiting: " << _waiting.size();
    lk.unlock();
    // must use t here, not owner because
    // lock has been released
    if (t) t->ready();
}

} // namespace
