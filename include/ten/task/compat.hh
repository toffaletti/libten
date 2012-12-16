#ifndef TEN_TASK_COMPAT
#define TEN_TASK_COMPAT

#include "ten/task/runtime.hh"
#include "ten/task/task_pimpl.hh"
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
typedef std::deque<task *> tasklist;

constexpr size_t default_stacksize = 256*1024;

//! spawn a new task in the current thread
inline uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize) {
    auto t = ten::task::spawn(f);
    return t.get_id();
}

//! current task id
inline uint64_t taskid() {
    return ten::this_task::get_id();
}
//! allow other tasks to run
inline void taskyield() {
    ten::this_task::yield();
}

//! cancel a task
inline bool taskcancel(uint64_t id) {
    // TODO: implement
    auto t = runtime::task_with_id(id);
    if (t) {
        t->cancel();
        return true;
    }
    return false;
}

//! mark the current task as a system task
inline void tasksystem() {
    // TODO: implement
}

//! set/get current task state
inline const char *taskstate(const char *fmt=nullptr, ...) {
    return "";
}
//! set/get current task name
inline const char * taskname(const char *fmt=nullptr, ...) {
    return "";
}

//! spawn a new thread with a task scheduler
inline void procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize) {
    std::thread proc([=]{
        f();
    });
    proc.detach();
}

//! cancel all non-system tasks and exit procmain
inline void procshutdown() {
    runtime::shutdown();
}

//! return cached time from event loop, not precise
inline std::chrono::time_point<std::chrono::steady_clock> procnow() {
    return runtime::now();
}

//! main entry point for tasks
struct procmain {
public:
    procmain() {}
    ~procmain() {}

    int main(int argc=0, char *argv[]=nullptr) {
        runtime::wait_for_all();
        return 0;
    }
};

//! sleep current task for milliseconds
inline void tasksleep(uint64_t ms) {
    ten::this_task::sleep_for(std::chrono::milliseconds{ms});
}

//! suspend task waiting for io on pollfds
inline int taskpoll(pollfd *fds, nfds_t nfds, uint64_t ms=0) {
    // TODO: implement
    ten::this_task::yield();
    return nfds;
}

//! suspend task waiting for io on fd
inline bool fdwait(int fd, int rw, uint64_t ms=0) {
    // TODO: implement
    ten::this_task::yield();
    return true;
}

} // compat
} // ten

#endif // TEN_TASK_COMPAT

