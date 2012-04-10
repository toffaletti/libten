#ifndef RENDEZ_HH
#define RENDEZ_HH

#include "task/qutex.hh"

namespace ten {

class rendez {
private:
    std::timed_mutex _m;
    tasklist _waiting;
public:
    rendez() {}
    rendez(const rendez &) = delete;
    rendez &operator =(const rendez &) = delete;

    ~rendez();

    void sleep(std::unique_lock<qutex> &lk);

    template <typename Predicate>
    void sleep(std::unique_lock<qutex> &lk, Predicate pred) {
        while (!pred()) {
            sleep(lk);
        }
    }

    void wakeup();
    void wakeupall();
};

} // namespace

#endif