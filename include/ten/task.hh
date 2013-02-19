#ifndef LIBTEN_TASK_HH
#define LIBTEN_TASK_HH

#include <chrono>
#include <functional>
#include <mutex>
#include <deque>
#include <memory>
#include <poll.h>
#include "ten/logging.hh"
#include "ten/optional.hh"
#include "ten/ptr.hh"

//! user must define
extern const size_t default_stacksize;

namespace ten {

extern void netinit();

using proc_clock_t = std::chrono::steady_clock;
using proc_time_t = std::chrono::time_point<proc_clock_t>;

//! return cached time from event loop, not precise
const proc_time_t &procnow();

//! exception to unwind stack on taskcancel
struct task_interrupted {};

typedef optional<std::chrono::milliseconds> optional_timeout;

namespace this_task {

//! id of the current task
uint64_t get_id();

//! allow other tasks to run
void yield();

void sleep_until(const proc_time_t& sleep_time);

template <class Rep, class Period>
    void sleep_for(std::chrono::duration<Rep, Period> sleep_duration) {
        sleep_until(procnow() + sleep_duration);
    }

} // end namespace this_task

class task {
public:
    struct pimpl;
    friend class proc;
private:
    std::shared_ptr<pimpl> _pimpl;

    explicit task(const std::function<void ()> &f);
public:
    task() {}
    task(const task &) = delete;
    task &operator=(const task &) = delete;
    task(task &&) = default;
    task &operator=(task &&) = default;

    //! spawn a new task in the current thread
    template<class Function, class... Args> 
        static task spawn(Function &&f, Args&&... args) {
            return task{std::bind(f, args...)};
        }

    //! id of this task
    uint64_t get_id() const;

public:
    static void set_default_stacksize(size_t stacksize);
};

class proc;

//! spawn a new task in the current thread
uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
//! current task id
uint64_t taskid();
//! allow other tasks to run
void taskyield();
//! cancel a task
bool taskcancel(uint64_t id);
//! mark the current task as a system task
void tasksystem();
//! set/get current task state
const char *taskstate(const char *fmt=nullptr, ...);
//! set/get current task name
const char * taskname(const char *fmt=nullptr, ...);

//! spawn a new thread with a task scheduler
uint64_t procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
//! cancel all non-system tasks and exit procmain
void procshutdown();

//! main entry point for tasks
struct procmain {
private:
    ptr<proc> p;
public:
    explicit procmain(std::shared_ptr<task::pimpl> t = nullptr);
    ~procmain();

    int main(int argc=0, char *argv[]=nullptr);
};

//! sleep current task for milliseconds
void tasksleep(uint64_t ms);
//! suspend task waiting for io on pollfds
int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms={});
//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, optional_timeout ms={});

// inherit from task_interrupted so lock/rendez/poll canceling
// doesn't need to be duplicated
struct deadline_reached : task_interrupted {};

// forward decl
struct deadline_pimpl;

//! schedule a deadline to interrupt task with
//! deadline_reached after milliseconds
class deadline {
private:
    std::unique_ptr<deadline_pimpl> _pimpl;
    void _set_deadline(std::chrono::milliseconds ms);
public:
    deadline(optional_timeout timeout);
    deadline(std::chrono::milliseconds ms); // deprecated

    deadline(const deadline &) = delete;
    deadline &operator =(const deadline &) = delete;

    //! milliseconds remaining on the deadline
    std::chrono::milliseconds remaining() const;
    
    //! cancel the deadline
    void cancel();

    ~deadline();
};

} // end namespace ten

#endif // LIBTEN_TASK_HH
