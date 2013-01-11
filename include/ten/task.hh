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

//! user must define
extern const size_t default_stacksize;

namespace ten {

extern void netinit();

//! exception to unwind stack on taskcancel
struct task_interrupted {};

typedef optional<std::chrono::milliseconds> optional_timeout;

struct task;
struct proc;
typedef std::deque<task *> tasklist;
typedef std::deque<proc *> proclist;

//! spawn a new task in the current thread
uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
//! current task id
uint64_t taskid();
//! allow other tasks to run
int64_t taskyield();
//! cancel a task
bool taskcancel(uint64_t id);
//! mark the current task as a system task
void tasksystem();
//! set/get current task state
const char *taskstate(const char *fmt=nullptr, ...);
//! set/get current task name
const char * taskname(const char *fmt=nullptr, ...);
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
