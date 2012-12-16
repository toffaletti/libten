#ifndef LIBTEN_TASK_HH
#define LIBTEN_TASK_HH

#include "ten/descriptors.hh"
#include "ten/logging.hh"
#include "ten/error.hh"

#include "ten/task/context.hh"

#include <chrono>
#include <functional>
#include <vector>
#include <atomic>

namespace ten {
// forward decl
class qutex;
class rendez;

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

private:
    friend class ten::rendez;
    friend class ten::qutex;
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
    std::exception_ptr _exception;
    std::atomic<bool> _ready;
    std::atomic<bool> _canceled;
    bool _unwinding = false;
#ifdef TEN_TASK_TRACE
    saved_backtrace _trace;
#endif

    static void trampoline(intptr_t arg);
private:
    static uint64_t next_id();

    task();
public:
    explicit task(std::function<void ()> f);

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

private:
    // TODO: private, compat
    void swap(bool nothrow = false);
    void safe_swap() noexcept {
        swap(true);
    }
public:
    // TODO: private
    void ready();
};

} // end namespace ten

#endif // LIBTEN_TASK_HH
