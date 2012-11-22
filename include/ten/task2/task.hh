#ifndef LIBTEN_TASK2_HH
#define LIBTEN_TASK2_HH

#include <chrono>
#include <functional>
#include <mutex>
#include <deque>
#include <memory>
#include <poll.h>
#include "logging.hh"

namespace ten {

namespace task2 {

//! exception to unwind stack on taskcancel
struct task_interrupted {};

class task {
public:
    //! spawn a new task in the current thread
    template <typename Func, typename Args...>
    static std::shared_ptr<task> spawn(Func &&f, Args ...args) {
        std::function<void ()> func = std::bind(f, args...);
    }

public:
    //! id of this task
    uint64_t get_id() const;
    //! cancel this task
    bool cancel();
    //! make the task a system task
    void detach();
};

namespace this_task {

//! id of the current task
uint64_t get_id() const;

//! allow other tasks to run
int64_t yield();

//! set/get current task state
const char *state(const char *fmt=nullptr, ...);

//! set/get current task name
const char * name(const char *fmt=nullptr, ...);

// TODO: sleep_for, sleep_until

//! sleep current task for milliseconds
void sleep(uint64_t ms);

//! suspend task waiting for io on pollfds
int poll(pollfd *fds, nfds_t nfds, uint64_t ms=0);

//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, uint64_t ms=0);



} // end namespace this_task

struct proc;

}

typedef std::deque<task *> tasklist;
typedef std::deque<proc *> proclist;

#if 0
//! get a dump of all task names and state for the current proc
std::string taskdump();
//! write task dump to FILE stream
void taskdumpf(FILE *of = stderr);

//! spawn a new thread with a task scheduler
uint64_t procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
//! cancel all non-system tasks and exit procmain
void procshutdown();

//! return cached time from event loop, not precise
const std::chrono::time_point<std::chrono::steady_clock> &procnow();

//! main entry point for tasks
struct procmain {
private:
    proc *p;
public:
    explicit procmain(task *t = nullptr);
    ~procmain();

    int main(int argc=0, char *argv[]=nullptr);
};
#endif

// inherit from task_interrupted so lock/rendez/poll canceling
// doesn't need to be duplicated
struct deadline_reached : task_interrupted {};

//! schedule a deadline to interrupt task with
//! deadline_reached after milliseconds
class deadline {
private:
    void *timeout_id;
public:
    deadline(std::chrono::milliseconds ms);

    deadline(const deadline &) = delete;
    deadline &operator =(const deadline &) = delete;

    //! milliseconds remaining on the deadline
    std::chrono::milliseconds remaining() const;
    
    //! cancel the deadline
    void cancel();

    ~deadline();
};

} // end namespace task2

} // end namespace ten

#endif // LIBTEN_TASK_HH
