#ifndef TASK_PRIVATE_HH
#define TASK_PRIVATE_HH

#include <memory>
#include <thread>
#include <atomic>

#include "ten/task.hh"
#include "ten/logging.hh"
#include "ten/error.hh"
#include "ten/ptr.hh"
#include "context.hh"

using namespace std::chrono;

namespace ten {

class scheduler;

void taskdumpf(FILE *of = stderr);

class task::pimpl {
    friend class scheduler;
    friend std::ostream &operator << (std::ostream &o, ptr<task::pimpl> t);
private:
    static constexpr size_t namesize = 16;
    static constexpr size_t statesize = 32;
public:
    struct cancellation_point {
        cancellation_point();
        ~cancellation_point();
    };
private:
    // order here is important
    // trying to get most used in the same cache line
    context _ctx;
    ptr<scheduler> _scheduler;
    std::exception_ptr exception;
    uint64_t cancel_points;
    char name[namesize];
    char state[statesize];
#ifdef TEN_TASK_TRACE
    saved_backtrace _trace;
#endif
public:
    const uint64_t id;
private:
    std::function<void ()> fn;
    std::atomic<bool> is_ready;
    bool canceled;
public:
    pimpl();
    pimpl(const std::function<void ()> &f, size_t stacksize);

    void setname(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void vsetname(const char *fmt, va_list arg);
    void setstate(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void vsetstate(const char *fmt, va_list arg);

    const char *getname() const { return name; }
    const char *getstate() const { return state; }

    void ready(bool front=false);
    void ready_for_io();

    void safe_swap() noexcept;
    void swap();

    void yield();

    void cancel();
    bool cancelable() const;

private:
    static void trampoline(intptr_t arg);
};

std::ostream &operator << (std::ostream &o, ptr<task::pimpl> t);

} // end namespace ten

#endif // TASK_PRIVATE_HH

