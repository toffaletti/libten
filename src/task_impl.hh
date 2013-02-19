#ifndef TASK_PRIVATE_HH
#define TASK_PRIVATE_HH

#include <memory>
#include <thread>
#include <atomic>

#include "ten/task.hh"
#include "ten/logging.hh"
#include "context.hh"

using namespace std::chrono;

namespace ten {

class scheduler;

void taskdumpf(FILE *of = stderr);

struct task::pimpl {
    static constexpr size_t namesize = 16;
    static constexpr size_t statesize = 32;
    struct cancellation_point {
        cancellation_point();
        ~cancellation_point();
    };

    context ctx;
    ptr<scheduler> _scheduler;
    std::exception_ptr exception;
    uint64_t cancel_points;
    std::unique_ptr<char[]> name;
    std::unique_ptr<char[]> state;
    uint64_t id;
    std::function<void ()> fn;
    std::atomic<bool> is_ready;
    bool systask;
    bool canceled;

    pimpl(const std::function<void ()> &f, size_t stacksize);

    void setname(const char *fmt, ...);
    void vsetname(const char *fmt, va_list arg);
    void setstate(const char *fmt, ...);
    void vsetstate(const char *fmt, va_list arg);

    void ready(bool front=false);
    void ready_for_io();

    void safe_swap() noexcept;
    void swap();

    void exit();
    void cancel();
    bool cancelable() const;

    static void start(intptr_t arg);
};

std::ostream &operator << (std::ostream &o, ptr<task::pimpl> t);

} // end namespace ten

#endif // TASK_PRIVATE_HH

