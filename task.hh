#ifndef TASK_HH
#define TASK_HH

#include <chrono>
#include <functional>
#include <mutex>
#include <deque>
#include <poll.h>
#include <boost/utility.hpp>
#include "logging.hh"

//! user must define
extern const size_t default_stacksize;

namespace ten {

using namespace std::chrono;

#if (__GNUC__ >= 4 && (__GNUC_MINOR__ >= 7))
typedef steady_clock monotonic_clock;
#endif

//! exception to unwind stack on taskcancel
struct task_interrupted {};

#define SEC2MS(s) (s*1000)

struct task;
struct proc;
typedef std::deque<task *> tasklist;
typedef std::deque<proc *> proclist;

uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
uint64_t taskid();
int64_t taskyield();
bool taskcancel(uint64_t id);
void tasksystem();
const char *taskstate(const char *fmt=0, ...);
const char * taskname(const char *fmt=0, ...);
std::string taskdump();
void taskdumpf(FILE *of = stderr);

uint64_t procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
void procshutdown();

// returns cached time, not precise
const time_point<monotonic_clock> &procnow();

struct qutex : boost::noncopyable {
    std::timed_mutex m;
    task *owner;
    tasklist waiting;

    qutex() {
        std::unique_lock<std::timed_mutex> lk(m);
        owner = 0;
    }
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

struct rendez : boost::noncopyable {
    std::timed_mutex m;
    tasklist waiting;

    rendez() {}
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

struct procmain {
    procmain();

    int main(int argc=0, char *argv[]=0);
};

void tasksleep(uint64_t ms);
int taskpoll(pollfd *fds, nfds_t nfds, uint64_t ms=0);
bool fdwait(int fd, int rw, uint64_t ms=0);

// inherit from task_interrupted so lock/rendez/poll canceling
// doesn't need to be duplicated
struct deadline_reached : task_interrupted {};
struct deadline {
    void *timeout_id;
    deadline(uint64_t milliseconds);
    ~deadline();
};

} // end namespace ten

#endif // TASK_HH

