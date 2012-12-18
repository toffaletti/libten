#ifndef TEN_TASK_PIMPL_HH
#define TEN_TASK_PIMPL_HH

#include "ten/descriptors.hh"
#include "ten/logging.hh"
#include "ten/error.hh"
#include "ten/task/task.hh"
#include "context.hh"
#include <chrono>
#include <atomic>

namespace ten {
// forward decl
class task;
class qutex;
class rendez;
class scheduler;
class thread_context;
class runtime;

class task_pimpl {
    friend std::ostream &operator << (std::ostream &o, const task_pimpl *t);
    friend class task;
    friend class thread_context;
    friend class scheduler;
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
    scheduler *_scheduler;
    std::function<void ()> _f;
    std::exception_ptr _exception;
    std::atomic<bool> _ready;
    std::atomic<bool> _canceled;
    bool _unwinding = false;
    char _name[32];
    char _state[128];
#ifdef TEN_TASK_TRACE
    saved_backtrace _trace;
#endif

    static void trampoline(intptr_t arg);
private:
    static uint64_t next_id();

    task_pimpl();

public:
    explicit task_pimpl(std::function<void ()> f);

    void setname(const char *fmt, ...) {
        va_list arg;
        va_start(arg, fmt);
        vsetname(fmt, arg);
        va_end(arg);
    }

    void vsetname(const char *fmt, va_list arg) {
        vsnprintf(_name, sizeof(_name), fmt, arg);
    }

    void setstate(const char *fmt, ...) {
        va_list arg;
        va_start(arg, fmt);
        vsetstate(fmt, arg);
        va_end(arg);
    }

    void vsetstate(const char *fmt, va_list arg) {
        vsnprintf(_state, sizeof(_state), fmt, arg);
    }

    const char *get_name() const { return _name; }
    const char *get_state() const { return _state; }

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

    //! cancel this task
    void cancel();

    uint64_t get_id() const { return _id; }

};

} // ten

#endif
