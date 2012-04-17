#ifndef QUTEX_HH
#define QUTEX_HH

#include "ten/task.hh"

namespace ten {

//! task aware mutex
class qutex {
private:
    std::timed_mutex _m;
    tasklist _waiting;
    task *_owner;
public:
    qutex() : _owner(0) {
        // a simple memory barrier would be sufficient here
        std::unique_lock<std::timed_mutex> lk(_m);
    }
    qutex(const qutex &) = delete;
    qutex &operator =(const qutex &) = delete;

    ~qutex() {}

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
private:
    void internal_unlock(
        std::unique_lock<std::timed_mutex> &lk);
};

} // namespace
#endif
