#ifndef TASK_PRIVATE_HH
#define TASK_PRIVATE_HH

#include <memory>
#include <thread>
#include <atomic>

#include "ten/task.hh"
#include "ten/logging.hh"
#include "ten/task/coroutine.hh"

using namespace std::chrono;

namespace ten {

struct task {
    struct cancellation_point {
        cancellation_point();
        ~cancellation_point();
    };

public:
    std::exception_ptr exception;
    char name[16];
    char state[32];
    std::function<void ()> fn;
    coroutine co;
    uint64_t id;
    proc *cproc;
    uint64_t _cancel_points;

    std::atomic<bool> _ready;
    bool systask;
    bool canceled;
    
public:
    task(const std::function<void ()> &f, size_t stacksize);
    void clear(bool newid=true);
    void init(const std::function<void ()> &f);
    ~task();

    void ready(bool front=false);
    void safe_swap() noexcept;
    void swap();

    void exit() {
        fn = nullptr;
        swap();
    }

    void cancel() {
        // don't cancel systasks
        if (systask) return;
        canceled = true;
        ready();
    }

    void setname(const char *fmt, ...) {
        va_list arg;
        va_start(arg, fmt);
        vsetname(fmt, arg);
        va_end(arg);
    }

    void vsetname(const char *fmt, va_list arg) {
        vsnprintf(name, sizeof(name), fmt, arg);
    }

    void setstate(const char *fmt, ...) {
        va_list arg;
        va_start(arg, fmt);
        vsetstate(fmt, arg);
        va_end(arg);
    }

    void vsetstate(const char *fmt, va_list arg) {
        vsnprintf(state, sizeof(state), fmt, arg);
    }

    static void start(void *arg) {
        task *t = (task *)arg;
        try {
            if (!t->canceled) {
                t->fn();
            }
        } catch (task_interrupted &e) {
            DVLOG(5) << t << " interrupted ";
        } catch (backtrace_exception &e) {
            LOG(ERROR) << "unhandled error in " << t << ": " << e.what() << "\n" << e.backtrace_str();
            std::exit(2);
        } catch (std::exception &e) {
            LOG(ERROR) << "unhandled error in " << t << ": " << e.what();
            std::exit(2);
        }
        t->exit();
    }

    friend std::ostream &operator << (std::ostream &o, task *t) {
        if (t) {
            o << "[" << (void*)t << " " << t->id << " "
                << t->name << " |" << t->state
                << "| sys: " << t->systask
                << " canceled: " << t->canceled << "]";
        } else {
            o << "nulltask";
        }
        return o;
    }

};

struct task_has_size {
    size_t stack_size;

    task_has_size(size_t stack_size_) : stack_size(stack_size_) {}

    bool operator()(const task *t) const {
        return t->co.stack_size() == stack_size;
    }
};

} // end namespace ten

#endif // TASK_PRIVATE_HH

