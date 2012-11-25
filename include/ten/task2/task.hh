#ifndef LIBTEN_TASK2_HH
#define LIBTEN_TASK2_HH

#include "ten/task2/coroutine.hh"

#include <chrono>
#include <functional>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>
#include <sys/syscall.h>


namespace ten {

namespace task2 {

class task;

class runtime {
private:
    static __thread runtime *_runtime;
    typedef std::shared_ptr<task> shared_task;
    coroutine _coro;
    std::vector<shared_task> _alltasks;
    std::deque<task *> _readyq;
public:
    runtime() {
        _runtime = this;
    }
    //! is this the main thread?
    static bool is_main_thread() noexcept {
        return getpid() == syscall(SYS_gettid);
    }

    //! spawn a new task in the current thread
    template<class Function, class... Args> 
    static std::shared_ptr<task> spawn(Function &&f, Args&&... args) {
        auto t = std::make_shared<task>(std::forward<Function>(f),
                std::forward<Args>(args)...);
        _runtime->_alltasks.push_back(t);
        _runtime->_readyq.push_back(t.get());
        return t;
    }

    void operator() ();
};

//! exception to unwind stack on taskcancel
struct task_interrupted {};

// forward decl
namespace this_task {
uint64_t get_id();
void yield();
} // this_task

class task {
private:
    friend class runtime;
    coroutine _coro;
    // when a task exits, its linked tasks are canceled
    //std::vector<std::shared_ptr<task>> _links;
    uint64_t _id;

    static uint64_t next_id();
public:
    //! create a new coroutine
    template<class Function, class... Args> 
    explicit task(Function &&f, Args&&... args)
    : _coro(std::forward<Function>(f), std::forward<Args>(args)...),
    _id(next_id())
    {
    }

public:
    //! id of this task
    uint64_t get_id() const { return _id; }
    //! cancel this task
    bool cancel();
    //! make the task a system task
    //void detach();
    //! join task
    void join();
protected:
    friend void this_task::yield();

    void yield();
};

namespace this_task {

//! id of the current task
uint64_t get_id();

//! allow other tasks to run
void yield();

//! set/get current task state
const char *state(const char *fmt=nullptr, ...);

//! set/get current task name
const char * name(const char *fmt=nullptr, ...);

// TODO: sleep_for, sleep_until

//! sleep current task for milliseconds
void sleep(uint64_t ms);

#if 0
//! suspend task waiting for io on pollfds
int poll(pollfd *fds, nfds_t nfds, uint64_t ms=0);

//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, uint64_t ms=0);
#endif


} // end namespace this_task

struct proc;


//typedef std::deque<task *> tasklist;
//typedef std::deque<proc *> proclist;

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
