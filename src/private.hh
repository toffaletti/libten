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
    struct timeout_t {
        time_point<steady_clock> when;
        std::exception_ptr exception;

        timeout_t(const time_point<steady_clock> &when_)
            : when(when_) {}

        friend std::ostream &operator <<(std::ostream &o, timeout_t *to) {
            o << "timeout[" << to->when.time_since_epoch().count() << "]";
            return o;
        }
    };

    struct task_timeout_ordering {
        bool operator ()(const timeout_t *a, const timeout_t *b) const {
            return a->when < b->when;
        }
    };

    char name[256];
    char state[256];
    std::function<void ()> fn;
    coroutine co;
    uint64_t id;
    proc *cproc;

    std::deque<timeout_t *> timeouts;

    bool exiting;
    bool systask;
    bool canceled;
    bool unwinding;
    
    task(const std::function<void ()> &f, size_t stacksize);
    void clear(bool newid=true);
    void init(const std::function<void ()> &f);
    ~task();

    template<typename Rep,typename Period, typename ExceptionT>
    timeout_t *add_timeout(const duration<Rep,Period> &dura, ExceptionT e) {
        std::unique_ptr<timeout_t> to(new timeout_t(procnow() + dura));
        to->exception = std::copy_exception(e);
        auto i = std::lower_bound(timeouts.begin(), timeouts.end(), to.get(), task_timeout_ordering());
        i = timeouts.insert(i, to.release());
        using ::operator <<;
        DVLOG(5) << "add timeout w/ ex task: " << this << " timeouts: " << timeouts;
        return *i;
    }

    template<typename Rep,typename Period>
    timeout_t *add_timeout(const duration<Rep,Period> &dura) {
        std::unique_ptr<timeout_t> to(new timeout_t(procnow() + dura));
        auto i = std::lower_bound(timeouts.begin(), timeouts.end(), to.get(), task_timeout_ordering());
        i = timeouts.insert(i, to.release());
        using ::operator <<;
        DVLOG(5) << "add timeout task: " << this << " timeouts: " << timeouts;
        return *i;
    }

    void remove_timeout(timeout_t *to);

    void ready();
    void swap();

    void exit() {
        fn = 0;
        exiting = true;
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
                << " exiting: " << t->exiting
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

