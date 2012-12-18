#ifndef LIBTEN_TASK_RENDEZ_HH
#define LIBTEN_TASK_RENDEZ_HH

#include "ten/task/qutex.hh"

namespace ten {

//! task aware condition rendezvous point
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

#endif // LIBTEN_TASK_RENDEZ_HH
