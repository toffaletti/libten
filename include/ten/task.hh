#ifndef LIBTEN_TASK_HH
#define LIBTEN_TASK_HH

#include <chrono>
#include <functional>
#include <mutex>
#include <deque>
#include <poll.h>
#include "logging.hh"

//! user must define
extern const size_t default_stacksize;

namespace ten {

extern void netinit();

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
const char *taskstate(const char *fmt=0, ...);
//! set/get current task name
const char * taskname(const char *fmt=0, ...);
//! get a dump of all task names and state for the current proc
std::string taskdump();
//! write task dump to FILE stream
void taskdumpf(FILE *of = stderr);

//! spawn a new thread with a task scheduler
uint64_t procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
//! cancel all non-system tasks and exit procmain
void procshutdown();

//! return cached time from event loop, not precise
const time_point<monotonic_clock> &procnow();

//! main entry point for tasks
struct procmain {
    procmain();

    int main(int argc=0, char *argv[]=0);
};

//! sleep current task for milliseconds
void tasksleep(uint64_t ms);
//! suspend task waiting for io on pollfds
int taskpoll(pollfd *fds, nfds_t nfds, uint64_t ms=0);
//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, uint64_t ms=0);

// inherit from task_interrupted so lock/rendez/poll canceling
// doesn't need to be duplicated
struct deadline_reached : task_interrupted {};

//! schedule a deadline to interrupt task with
//! deadline_reached after milliseconds
struct deadline {
    void *timeout_id;
    deadline(milliseconds ms);

    deadline(const deadline &) = delete;
    deadline &operator =(const deadline &) = delete;

    ~deadline();
};

} // end namespace ten

#endif // LIBTEN_TASK_HH
