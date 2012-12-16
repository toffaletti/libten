#include "ten/task/task.hh"
#include "ten/task/task_pimpl.hh"

#include "ten/thread_local.hh"
#include "ten/llqueue.hh"
#include "ten/task/alarm.hh"
#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <sys/syscall.h>

namespace ten {

class runtime {
public:
    typedef std::chrono::steady_clock clock;
    typedef std::chrono::time_point<clock> time_point;
    typedef std::shared_ptr<task_pimpl> shared_task;
    typedef ten::alarm_clock<task_pimpl *, clock> alarm_clock;
private:
    friend class task;
    friend class task_pimpl::cancellation_point;
    friend class deadline;
    friend void task_pimpl::ready();
    friend void task_pimpl::yield();
    friend void task_pimpl::swap(bool nothrow);
    friend void task_pimpl::trampoline(intptr_t arg);
    friend void task_pimpl::cancel();

    friend uint64_t this_task::get_id();
    friend void this_task::yield();
    template<class Rep, class Period>
        friend void this_task::sleep_for(std::chrono::duration<Rep, Period> sleep_duration);
    template <class Clock, class Duration>
        friend void this_task::sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time);

public:
    // TODO: should be private
    static task_pimpl *current_task();

private:
    task_pimpl _task;
    task_pimpl *_current_task = nullptr;
    std::vector<shared_task> _alltasks;
    std::vector<shared_task> _gctasks;
    std::deque<task_pimpl *> _readyq;
    //! current time cached in a few places through the event loop
    time_point _now;
    llqueue<task_pimpl *> _dirtyq;
    alarm_clock _alarms;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _canceled;
    bool _tasks_canceled = false;

    const time_point &update_cached_time() {
        _now = clock::now();
        return _now;
    }

    template <class Duration>
        static void sleep_until(const std::chrono::time_point<clock, Duration>& sleep_time) {
            runtime *r = thread_local_ptr<runtime>();
            task_pimpl *t = r->_current_task;
            alarm_clock::scoped_alarm sleep_alarm(r->_alarms, t, sleep_time);
            task_pimpl::cancellation_point cancelable;
            t->swap();
        }

    void ready(task_pimpl *t);
    void schedule();
    void check_dirty_queue();
    void check_timeout_tasks();
    void remove_task(task_pimpl *t);
    void cancel();
    int dump();
    static int dump_all();
public:
    runtime();
    ~runtime();

    //! is this the main thread?
    static bool is_main_thread() noexcept {
        return getpid() == syscall(SYS_gettid);
    }

    static time_point now() { return thread_local_ptr<runtime>()->_now; }

    // compat
    static shared_task task_with_id(uint64_t id) {
        runtime *r = thread_local_ptr<runtime>();
        for (auto t : r->_alltasks) {
            if (t->_id == id) return t;
        }
        return nullptr;
    }

    static void wait_for_all();

    static void shutdown();
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
    runtime::alarm_clock::scoped_alarm _alarm;
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

} // ten

