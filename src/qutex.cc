#include "ten/task/qutex.hh"
#include "thread_context.hh"

namespace ten {

void qutex::safe_lock() noexcept {
    // NOTE: copy-pasted logic from lock() but calls safe_swap which doesn't throw
    // this is useful if you need to lock inside a destructor
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    DCHECK(!t->cancelable()) << "BUG: cannot cancel a lock";
    DCHECK(t) << "BUG: qutex::lock called outside of task";
    {
        std::lock_guard<std::timed_mutex> lk{_m};
        DCHECK(_owner != t) << "no recursive locking: " << t;
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
        DCHECK(!t->cancelable()) << "BUG: cannot cancel a lock";
        t->safe_swap(); // don't allow swap to throw on deadline timeout
        std::lock_guard<std::timed_mutex> lk{_m};
        if (_owner == t) {
            break;
        }
    }
}

void qutex::lock() {
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    DCHECK(!t->cancelable()) << "BUG: cannot cancel a lock";
    DCHECK(t) << "BUG: qutex::lock called outside of task";
    {
        std::lock_guard<std::timed_mutex> lk{_m};
        DCHECK(_owner != t) << "no recursive locking: " << t;
        if (_owner == nullptr) {
            _owner = t;
            DVLOG(5) << "LOCK qutex: " << this << " owner: " << _owner;
            return;
        }
        DVLOG(5) << "QUTEX[" << this << "] lock waiting add: " << t <<  " owner: " << _owner;
        _waiting.push_back(t);
    }

    // loop to handle spurious wakeups from other threads
    try {
        for (;;) {
            DCHECK(!t->cancelable()) << "BUG: cannot cancel a lock";
            t->swap();
            std::lock_guard<std::timed_mutex> lk{_m};
            if (_owner == t) {
                break;
            }
        }
    } catch (...) {
        // deadline timeouts can trigger this
        std::unique_lock<std::timed_mutex> lk{_m};
        unlock_or_giveup(lk);
        throw;
    }
}

bool qutex::try_lock() {
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
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

inline void qutex::unlock_or_giveup(std::unique_lock<std::timed_mutex> &lk) {
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    DCHECK(lk.owns_lock()) << "BUG: lock not owned " << t;
    DVLOG(5) << "QUTEX[" << this << "] unlock: " << t;
    if (t == _owner) {
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
    } else {
        // this branch is taken when exception is thrown inside
        // a task that is currently waiting inside qutex::lock
        // give up trying to acquire the lock
        auto i = std::find(_waiting.begin(), _waiting.end(), t);
        if (i != _waiting.end()) {
            _waiting.erase(i);
        }
    }
}

void qutex::unlock() {
    std::unique_lock<std::timed_mutex> lk{_m};
    unlock_or_giveup(lk);
}

} // namespace
