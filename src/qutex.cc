#include "ten/task/qutex.hh"
#include "proc.hh"

namespace ten {

using std::timed_mutex;
using std::unique_lock;
using std::try_to_lock;

void qutex::lock() {
    task *t = _this_proc->ctask;
    CHECK(t) << "BUG: qutex::lock called outside of task";
    {
        unique_lock<timed_mutex> lk(_m);
        if (_owner == 0 || _owner == t) {
            _owner = t;
            DVLOG(5) << "LOCK qutex: " << this << " owner: " << _owner;
            return;
        }
        DVLOG(5) << "QUTEX[" << this << "] lock waiting add: " << t <<  " owner: " << _owner;
        _waiting.push_back(t);
    }

    try {
        // loop to handle spurious wakeups from other threads
        for (;;) {
            t->swap();
            unique_lock<timed_mutex> lk(_m);
            if (_owner == _this_proc->ctask) {
                break;
            }
        }
    } catch (...) {
        unique_lock<timed_mutex> lk(_m);
        internal_unlock(lk);
        throw;
    }
}

bool qutex::try_lock() {
    task *t = _this_proc->ctask;
    CHECK(t) << "BUG: qutex::try_lock called outside of task";
    unique_lock<timed_mutex> lk(_m, try_to_lock);
    if (lk.owns_lock()) {
        if (_owner == 0) {
            _owner = t;
            return true;
        }
    }
    return false;
}

void qutex::unlock() {
    unique_lock<timed_mutex> lk(_m);
    internal_unlock(lk);
}

void qutex::internal_unlock(unique_lock<timed_mutex> &lk) {
    task *t = _this_proc->ctask;
    CHECK(lk.owns_lock()) << "BUG: lock not owned " << t;
    DVLOG(5) << "QUTEX[" << this << "] unlock: " << t;
    if (t == _owner) {
        if (!_waiting.empty()) {
            t = _owner = _waiting.front();
            _waiting.pop_front();
        } else {
            t = _owner = 0;
        }
        DVLOG(5) << "UNLOCK qutex: " << this
            << " new owner: " << _owner
            << " waiting: " << _waiting.size();
        lk.unlock();
        // must use t here, not owner because
        // lock has been released
        if (t) t->ready();
    } else {
        // this branch is taken when exception is thrown inside
        // a task that is currently waiting inside qutex::lock
        auto i = std::find(_waiting.begin(), _waiting.end(), t);
        if (i != _waiting.end()) {
            _waiting.erase(i);
        }
    }
}

} // namespace
