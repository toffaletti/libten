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

class task::impl {
    friend class scheduler;
    friend std::ostream &operator << (std::ostream &o, ptr<task::impl> t);
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
    std::exception_ptr _exception;
    uint64_t _cancel_points;
    struct auxinfo { char name[namesize]; char state[statesize]; };
    std::unique_ptr<auxinfo> _aux;
#ifdef TEN_TASK_TRACE
    saved_backtrace _trace;
#endif
    const uint64_t _id;
    std::function<void ()> _fn;
    std::atomic<bool> _ready;
    std::atomic<bool> _canceled;
public:
    impl();
    impl(const std::function<void ()> &f, size_t stacksize);

    void setname(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void vsetname(const char *fmt, va_list arg);
    void setstate(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void vsetstate(const char *fmt, va_list arg);

    const char *getname() const { return _aux->name; }
    const char *getstate() const { return _aux->state; }

    void ready(bool front=false);
    void ready_for_io();

    void safe_swap() noexcept;
    void swap();

    void yield();

    void cancel();
    bool cancelable() const;

    uint64_t get_id() const { return _id; }

private:
    static void trampoline(intptr_t arg);
};

std::ostream &operator << (std::ostream &o, ptr<task::impl> t);

} // end namespace ten

#endif // TASK_PRIVATE_HH

