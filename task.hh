#ifndef TASK_HH
#define TASK_HH

#include <functional>
#include <mutex>
#include <deque>

//! user must define
extern size_t default_stacksize;

namespace fw {

#define SEC2MS(s) (s*1000)

struct task;
struct proc;
typedef std::deque<task *> tasklist;
typedef std::deque<proc *> proclist;

uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
int64_t taskyield();
//static void taskexit(void *r = 0);
void tasksystem();
void tasksetstate(const char *fmt, ...);
const char *taskgetstate();
void tasksetname(const char *fmt, ...);
const char *taskgetname();

uint64_t procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);

struct qutex {
    std::mutex m;
    task *owner;
    tasklist waiting;

    qutex() {
        std::unique_lock<std::mutex> lk(m);
        owner = 0;
    }
    void lock();
    void unlock();
};

struct rendez {
    qutex q;
    tasklist waiting;

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
bool fdwait(int fd, int rw, uint64_t ms=0);

} // end namespace fw

#endif // TASK_HH

