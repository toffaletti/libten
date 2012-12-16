#ifndef TEN_RUNTIME_HH
#define TEN_RUNTIME_HH

#include "ten/task/task.hh"
#include "ten/task/alarm.hh"
#include "ten/llqueue.hh"
#include <memory>

#include <unistd.h>
#include <sys/syscall.h>

namespace ten {

struct task_pimpl;

class runtime {
public:
    typedef std::chrono::steady_clock clock;
    typedef std::chrono::time_point<clock> time_point;
    typedef std::shared_ptr<task_pimpl> shared_task;
    typedef ten::alarm_clock<task_pimpl *, clock> alarm_clock;
private:
    template<class Rep, class Period>
        friend void this_task::sleep_for(std::chrono::duration<Rep, Period> sleep_duration);
    template <class Clock, class Duration>
        friend void this_task::sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time);

public:
    // TODO: should be private
    static task_pimpl *current_task();
    static void attach(shared_task t);
private:

    static int dump_all();

    static void sleep_until(const time_point &sleep_time);
public:

    //! is this the main thread?
    static bool is_main_thread() noexcept {
        return getpid() == syscall(SYS_gettid);
    }

    static time_point now();

    // compat
    static shared_task task_with_id(uint64_t id);

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

#endif
