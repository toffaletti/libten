#ifndef TEN_TASK_PIMPL_HH
#define TEN_TASK_PIMPL_HH

#include "ten/descriptors.hh"
#include "ten/logging.hh"
#include "ten/error.hh"
#include "ten/task/task.hh"
#include "ten/task/context.hh"
#include <chrono>
#include <atomic>

namespace ten {
// forward decl
class task;
class qutex;
class rendez;
class runtime;
class thread_context;

class task_pimpl {
    friend std::ostream &operator << (std::ostream &o, const task_pimpl *t);
    friend class task;
    friend class thread_context;
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
    thread_context *_runtime; // TODO: scheduler
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

    task_pimpl();

public:
    explicit task_pimpl(std::function<void ()> f);

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
