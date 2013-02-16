#ifndef LIBTEN_TASK_QUTEX_HH
#define LIBTEN_TASK_QUTEX_HH

#include "ten/task/runtime.hh"
#include <mutex>

namespace ten {

// almost surely, Mutex = qutex
template <typename Mutex>
class safe_lock {
    Mutex &_mut;

public:
    safe_lock(Mutex &mut) noexcept : _mut(mut) {
        _mut.safe_lock();
    }
    ~safe_lock() noexcept {
        _mut.unlock();
    }

    //! no copy (and no move)
    safe_lock(const safe_lock &) = delete;
    safe_lock & operator = (const safe_lock &) = delete;
};

//! task aware mutex
class qutex {
private:
    std::timed_mutex _m;
    tasklist _waiting;
    task_pimpl *_owner;

    void unlock_or_giveup(std::unique_lock<std::timed_mutex> &lk);
public:
    qutex() : _owner(nullptr) {
        // a simple memory barrier would be sufficient here
        std::unique_lock<std::timed_mutex> lk(_m);
    }
    qutex(const qutex &) = delete;
    qutex &operator =(const qutex &) = delete;

    ~qutex() {}

    void safe_lock() noexcept;
    void lock();
    void unlock();
    bool try_lock();
    template<typename Rep,typename Period>
        bool try_lock_for(
                std::chrono::duration<Rep,Period> const&
                relative_time);
    template<typename Clock,typename Duration>
        bool try_lock_until(
                std::chrono::time_point<Clock,Duration> const&
                absolute_time);
};

} // namespace

#endif // LIBTEN_TASK_QUTEX_HH
