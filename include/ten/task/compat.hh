#ifndef LIBTEN_TASK_COMPAT_HH
#define LIBTEN_TASK_COMPAT_HH

//! user must define
extern const size_t default_stacksize;

namespace ten {

extern void netinit();

using proc_clock_t = kernel::clock;
using proc_time_t = kernel::time_point;

//! return cached time from event loop, not precise
proc_time_t procnow();

//! spawn a new task in the current thread
uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
//! current task id
uint64_t taskid();
//! allow other tasks to run
void taskyield();
//! cancel a task
bool taskcancel(uint64_t id);
//! set/get current task state
const char *taskstate(const char *fmt=nullptr, ...);
//! set/get current task name
const char * taskname(const char *fmt=nullptr, ...);

//! spawn a new thread with a task scheduler
void procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
//! cancel all non-system tasks and exit procmain
void procshutdown();

//! main entry point for tasks
struct procmain {
public:
    procmain();

    int main(int argc=0, char *argv[]=nullptr);
};

//! sleep current task for milliseconds
void tasksleep(uint64_t ms);
//! suspend task waiting for io on pollfds
int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms=nullopt);
//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, optional_timeout ms=nullopt);

} // ten

#endif
