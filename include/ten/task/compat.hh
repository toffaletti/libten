#ifndef TEN_TASK_COMPAT
#define TEN_TASK_COMPAT

#include "ten/task/task.hh"
#include "ten/task/runtime.hh"
#include "ten/task/deadline.hh"
#include <poll.h>
#include <thread>
#include <deque>

#define SEC2MS(s) (s*1000)

namespace ten {
namespace compat {

typedef ten::task_interrupted task_interrupted;
typedef ten::task_pimpl task;
typedef ten::deadline_reached deadline_reached;
typedef ten::deadline deadline;

//! spawn a new task in the current thread
uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize=0);

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
void procspawn(const std::function<void ()> &f, size_t stacksize=0);

//! cancel all non-system tasks and exit procmain
void procshutdown();

using proc_clock_t = std::chrono::steady_clock;
using proc_time_t = std::chrono::time_point<proc_clock_t>;
//! return cached time from event loop, not precise
proc_time_t procnow();

//! main entry point for tasks
struct procmain {
public:
    procmain();
    ~procmain() {}

    int main(int argc=0, char *argv[]=nullptr) {
        runtime::wait_for_all();
        return 0;
    }
};

//! sleep current task for milliseconds
void tasksleep(uint64_t ms);

//! suspend task waiting for io on pollfds
int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms={});

//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, optional_timeout ms={});

} // compat
} // ten

#endif // TEN_TASK_COMPAT

