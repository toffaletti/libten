#ifndef LIBTEN_TASK2_HH
#define LIBTEN_TASK2_HH

#include "ten/descriptors.hh"
#include "ten/llqueue.hh"
#include "ten/logging.hh"
#include "ten/thread_local.hh"
#include "ten/error.hh"

#include "ten/task2/context.hh"
#include "ten/task2/alarm.hh"

#include <chrono>
#include <functional>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>
#include <condition_variable>
#include <sys/syscall.h>

namespace ten {
namespace task2 {

//! exception to unwind stack on taskcancel
struct task_interrupted {};

// forward decl
namespace this_task {
uint64_t get_id();
void yield();

template<class Rep, class Period>
    void sleep_for(std::chrono::duration<Rep, Period> sleep_duration);

template <class Clock, class Duration>
    void sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time);

} // this_task

class runtime;

class task {
    friend std::ostream &operator << (std::ostream &o, const task *t);
    friend class runtime;
public:
    typedef std::chrono::steady_clock clock;
    typedef std::chrono::time_point<clock> time_point;

    enum class state {
        fresh,
        ready,
        asleep,
        canceled,
        unwinding,
        finished
    };

private:
    struct cancellation_point {
        cancellation_point();
        ~cancellation_point();
    };

private:
    // when a task exits, its linked tasks are canceled
    //std::vector<std::shared_ptr<task>> _links;
    context _ctx;
    uint64_t _id;
    uint64_t _cancel_points = 0;
    runtime *_runtime; // TODO: scheduler
    std::function<void ()> _f;
    std::atomic<state> _state;
    std::exception_ptr _exception;
#ifdef TEN_TASK_TRACE
    saved_backtrace _trace;
#endif

    static void trampoline(intptr_t arg);
private:
    static uint64_t next_id();

    task() : _ctx(), _id{next_id()}, _state(state::ready) {}
public:
    //! create a new task
    template<class Function, class... Args> 
        explicit task(Function &&f, Args&&... args)
        : _ctx{task::trampoline},
        _id{next_id()},
        _f{std::bind(f, args...)},
        _state(state::fresh)
    {
    }

public:
    //! id of this task
    uint64_t get_id() const { return _id; }
    //! cancel this task
    void cancel();
    //! make the task a system task
    //void detach();
    //! join task
    void join();

private:
    friend class deadline;
    friend uint64_t this_task::get_id();
    friend void this_task::yield();

    void yield();
    void post_swap();
    bool transition(state to);
    bool runnable() const {
        state st = _state;
        return (st == state::fresh || st == state::ready || st == state::canceled || st == state::unwinding);
    }
};

class runtime {
public:
    typedef std::chrono::steady_clock clock;
    typedef std::chrono::time_point<clock> time_point;
    typedef std::shared_ptr<task> shared_task;
    typedef alarm_set<task *, clock> alarm_set_type;
private:
    friend class task::cancellation_point;
    friend class deadline;
    friend void task::post_swap();
    friend void task::trampoline(intptr_t arg);
    friend void task::yield();
    friend void task::cancel();
    friend uint64_t this_task::get_id();
    friend void this_task::yield();
    template<class Rep, class Period>
        friend void this_task::sleep_for(std::chrono::duration<Rep, Period> sleep_duration);
    template <class Clock, class Duration>
        friend void this_task::sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time);

    static task *current_task();

private:
    task _task;
    task *_current_task = nullptr;
    std::vector<shared_task> _alltasks;
    std::vector<shared_task> _gctasks;
    std::deque<task *> _readyq;
    //! current time cached in a few places through the event loop
    time_point _now;
    llqueue<task *> _dirtyq;
    alarm_set_type _alarms;
    std::mutex _mutex;
    std::condition_variable _cv;

    const time_point &update_cached_time() {
        _now = clock::now();
        return _now;
    }

    template <class Duration>
        static void sleep_until(const std::chrono::time_point<clock, Duration>& sleep_time) {
            runtime *r = thread_local_ptr<runtime>();
            task *t = r->_current_task;
            t->transition(task::state::asleep);
            alarm_set_type::alarm alarm(r->_alarms, t, sleep_time);
            task::cancellation_point cancelable;
            r->schedule();
        }

    void ready(task *t);
    void schedule();
    void check_dirty_queue();
    void check_timeout_tasks();
    void remove_task(task *t);
    
public:
    runtime() {
        // TODO: reenable this when our libstdc++ is fixed
        static_assert(clock::is_steady, "clock not steady");
        update_cached_time();
        _task._runtime = this;
        //_alltasks.push_back(_task);
        _task.transition(task::state::ready);
        //_readyq.push_back(_task.get());
        _current_task = &_task;
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
        runtime *r = thread_local_ptr<runtime>();
        t->_runtime = r;
        r->_alltasks.push_back(t);
        DVLOG(5) << "spawn readyq " << t;
        r->_readyq.push_back(t.get());
        return t;
    }

    static time_point now() { return thread_local_ptr<runtime>()->_now; }

    static int dump();

    static void wait_for_all() {
        runtime *r = thread_local_ptr<runtime>();
        // TODO: fix this hack
        while (!r->_alltasks.empty()) {
            this_task::yield();
        }
    }

    static void shutdown() {
        if (is_main_thread()) {
            runtime *r = thread_local_ptr<runtime>();
            r->~runtime();
        }
    }

};

namespace this_task {

//! id of the current task
uint64_t get_id();

//! allow other tasks to run
void yield();

template<class Rep, class Period>
    void sleep_for(std::chrono::duration<Rep, Period> sleep_duration) {
        runtime::sleep_until(runtime::now() + sleep_duration);
    }

template <class Clock, class Duration>
    void sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time) {
        runtime::sleep_until(sleep_time);
    }

//! set/get current task state
const char *state(const char *fmt=nullptr, ...);

//! set/get current task name
const char * name(const char *fmt=nullptr, ...);

} // end namespace this_task

// inherit from task_interrupted so lock/rendez/poll canceling
// doesn't need to be duplicated
struct deadline_reached : task_interrupted {};

//! schedule a deadline to interrupt task with
//! deadline_reached after milliseconds
class deadline {
private:
    runtime::alarm_set_type::alarm _alarm;
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
