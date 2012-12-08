#include "ten/task2/task.hh"

namespace ten {
namespace task2 {

constexpr size_t default_stacksize = 256*1024;

//! spawn a new task in the current thread
uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize) {
    auto t = runtime::spawn(f);
    return t->get_id();
}

//! current task id
uint64_t taskid() {
    return this_task::get_id();
}
//! allow other tasks to run
int64_t taskyield() {
    this_task::yield();
    return 0;
}

//! cancel a task
bool taskcancel(uint64_t id) {
    // TODO: implement
    return false;
}

//! mark the current task as a system task
void tasksystem() {
    // TODO: implement
}

//! set/get current task state
const char *taskstate(const char *fmt=nullptr, ...) {
    return "";
}
//! set/get current task name
const char * taskname(const char *fmt=nullptr, ...) {
    return "";
}

//! spawn a new thread with a task scheduler
uint64_t procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize) {
    // TODO: implement
    return 0;
}

//! cancel all non-system tasks and exit procmain
void procshutdown() {
    // TODO: implement
}

//! return cached time from event loop, not precise
std::chrono::time_point<std::chrono::steady_clock> procnow() {
    return runtime::now();
}

} // task2
} // ten
