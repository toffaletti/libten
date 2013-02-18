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

class task {
private:
    struct pimpl;
    std::shared_ptr<pimpl> _pimpl;
public:
    struct cancellation_point {
        cancellation_point();
        ~cancellation_point();
    };

    bool cancelable() const;
public:
    task(const std::function<void ()> &f, size_t stacksize);
    ~task();

    uint64_t id() const;

private:
    void ready(bool front=false);
    void safe_swap() noexcept;
    void swap();

    void clear(bool newid=true);
    void init(const std::function<void ()> &f);

    void exit();
    void cancel();

    static void start(intptr_t arg);

    friend std::ostream &operator << (std::ostream &o, ptr<task> t);
    friend class proc;
    friend struct io_scheduler;
    friend uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize);
    friend uint64_t taskid();
    friend const char *taskname(const char *fmt, ...);
    friend const char *taskstate(const char *fmt, ...);
    friend int64_t taskyield();
    friend class rendez;
    friend class qutex;
};

struct task::pimpl {
    std::exception_ptr exception;
    char name[16];
    char state[32];
    std::function<void ()> fn;
    context ctx;
    uint64_t id;
    ptr<proc> cproc;
    uint64_t cancel_points;
    std::atomic<bool> ready;
    bool systask;
    bool canceled;

    pimpl(const std::function<void ()> &f, size_t stacksize)
        : ctx{task::start, stacksize}
    {
        clear();
        fn = f;
    }

    void setname(const char *fmt, ...);
    void vsetname(const char *fmt, va_list arg);
    void setstate(const char *fmt, ...);
    void vsetstate(const char *fmt, va_list arg);

    void clear(bool newid=true);
};

} // end namespace ten

#endif // TASK_PRIVATE_HH

