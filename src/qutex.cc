#include "ten/task/qutex.hh"
#include "scheduler.hh"

namespace ten {

void qutex::lock(lock_type_t lt) {
    const auto t = scheduler::current_task();
    DCHECK(!t->cancelable()) << "BUG: cannot cancel a lock";
    DCHECK(t) << "BUG: qutex::lock called outside of task";
    {
        std::lock_guard<std::mutex> lk{_m};
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
            switch (lt) {
                case safe_lock:
                    t->safe_swap(); // don't allow swap to throw on deadline timeout
                    break;
                case interruptable_lock:
                default:
                    t->swap();
            }
            std::lock_guard<std::mutex> lk{_m};
            if (_owner == t) {
                break;
            }
        }
    } catch (...) {
        // deadline timeouts can trigger this
        std::lock_guard<std::mutex> lk{_m};
        unlock_or_giveup(lk);
        throw;
    }
}

bool qutex::try_lock() {
    const auto t = scheduler::current_task();
    DCHECK(t) << "BUG: qutex::try_lock called outside of task";
    std::unique_lock<std::mutex> lk{_m, std::try_to_lock};
    if (lk.owns_lock()) {
        if (_owner == nullptr) {
            _owner = t;
            return true;
        }
    }
    return false;
}

void qutex::unlock_or_giveup(std::lock_guard<std::mutex> &lk) noexcept {
    const auto current_task = scheduler::current_task();
    DVLOG(5) << "QUTEX[" << this << "] unlock: " << current_task;
    if (current_task == _owner) {
        ptr<task::impl> new_owner;
        if (!_waiting.empty()) {
            new_owner = _waiting.front();
            _waiting.pop_front();
        }
        _owner = new_owner;
        DVLOG(5) << "UNLOCK qutex: " << this
            << " new owner: " << new_owner
            << " waiting: " << _waiting.size();
        if (new_owner) new_owner->ready();
    } else {
        // this branch is taken when exception is thrown inside
        // a task that is currently waiting inside qutex::lock
        // give up trying to acquire the lock
        auto i = std::find(_waiting.begin(), _waiting.end(), current_task);
        if (i != _waiting.end()) {
            _waiting.erase(i);
        }
    }
}

void qutex::unlock() {
    std::lock_guard<std::mutex> lk{_m};
    unlock_or_giveup(lk);
}

} // namespace
