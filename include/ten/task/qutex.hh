#ifndef LIBTEN_TASK_QUTEX_HH
#define LIBTEN_TASK_QUTEX_HH

#include "ten/task.hh"
#include "ten/ptr.hh"
#include <mutex>
#include <deque>

namespace ten {

//! task aware mutex
class qutex {
private:
    std::mutex _m;
    std::deque<ptr<task::impl>> _waiting;
    ptr<task::impl> _owner;

    void unlock_or_giveup(std::lock_guard<std::mutex> &lk) noexcept;
public:
    enum lock_type_t { interruptable_lock, safe_lock };

    qutex() : _owner(nullptr) {
        // a simple memory barrier would be sufficient here
        std::unique_lock<std::mutex> lk(_m);
    }
    qutex(const qutex &) = delete;
    qutex &operator =(const qutex &) = delete;

    ~qutex() {}

    void lock(lock_type_t lt = interruptable_lock);
    void unlock();
    bool try_lock();
};

// almost surely, Mutex = qutex
template <typename Mutex>
class safe_lock {
    Mutex &_mut;

public:
    safe_lock(Mutex &mut) noexcept : _mut(mut) {
        _mut.lock(qutex::safe_lock);
    }
    ~safe_lock() noexcept {
        _mut.unlock();
    }

    //! no copy (and no move)
    safe_lock(const safe_lock &) = delete;
    safe_lock & operator = (const safe_lock &) = delete;
};

} // namespace

#endif // LIBTEN_TASK_QUTEX_HH
