#include "ten/task/compat.hh"
#include "task_pimpl.hh"

namespace ten {
namespace compat {

//! spawn a new task in the current thread
uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    auto t = ten::task::spawn(f);
    return t.get_id();
}

//! current task id
uint64_t taskid() {
    return ten::this_task::get_id();
}
//! allow other tasks to run
void taskyield() {
    ten::this_task::yield();
}

//! cancel a task
bool taskcancel(uint64_t id) {
    // TODO: implement
    auto t = runtime::task_with_id(id);
    if (t) {
        t->cancel();
        return true;
    }
    return false;
}

//! mark the current task as a system task
void tasksystem() {
    // TODO: implement
}

//! set/get current task state
const char *taskstate(const char *fmt, ...) {
    return "";
}
//! set/get current task name
const char * taskname(const char *fmt, ...) {
    return "";
}

//! spawn a new thread with a task scheduler
void procspawn(const std::function<void ()> &f, size_t stacksize) {
    std::thread proc([=]{
        f();
    });
    proc.detach();
}

//! cancel all non-system tasks and exit procmain
void procshutdown() {
    runtime::shutdown();
}

//! return cached time from event loop, not precise
std::chrono::time_point<std::chrono::steady_clock> procnow() {
    return runtime::now();
}


//! sleep current task for milliseconds
void tasksleep(uint64_t ms) {
    ten::this_task::sleep_for(std::chrono::milliseconds{ms});
}

//! suspend task waiting for io on pollfds
int taskpoll(pollfd *fds, nfds_t nfds, uint64_t ms) {
    // TODO: implement
    ten::this_task::yield();
    return nfds;
}

//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, uint64_t ms) {
    // TODO: implement
    ten::this_task::yield();
    return true;
}

} // compat
} // ten
