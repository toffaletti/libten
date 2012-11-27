#ifndef LIBTEN_TASK2_HH
#define LIBTEN_TASK2_HH

#include "ten/descriptors.hh"
#include "ten/llqueue.hh"
#include "ten/logging.hh"

#include "ten/task2/coroutine.hh"

#include <chrono>
#include <functional>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>
#include <condition_variable>
#include <sys/syscall.h>


namespace ten {

namespace task2 {

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

class task : private coroutine {
    friend class runtime;
public:
    typedef std::chrono::steady_clock clock;
    typedef std::chrono::time_point<clock> time_point;

    enum class state {
        ready,
        asleep,
        canceled,
        canceling
    };
private:
    struct cancellation_point {
        cancellation_point();
        ~cancellation_point();
    };

private:
    struct timeout {
        time_point when;
        std::exception_ptr exception;
    };

    struct timeout_set {
    private:
        std::deque<timeout *> _set;
    private:
        struct order {
            bool operator ()(const timeout *a, const timeout *b) const {
                return a->when < b->when;
            }
        };

        timeout *insert(std::unique_ptr<timeout> &&to) {
            auto i = std::lower_bound(std::begin(_set),
                   std::end(_set), to.get(), order());
            i = _set.insert(i, to.release());
            using ::operator <<;
            //DVLOG(5) << "add timeout w/ ex task: " << this << " timeouts: " << timeouts;
            return *i;
        }

    public:
        template<typename ExceptionT>
        timeout *insert(time_point when, ExceptionT e) {
            std::unique_ptr<timeout> to{new timeout{
                    when,
                    std::copy_exception(e)}
            };
            return insert(std::move(to));
        }
        
        timeout *insert(time_point when) {
            std::unique_ptr<timeout> to{new timeout{when}};
            return insert(std::move(to));
        }

        void remove(timeout *to) {
            auto i = std::find(std::begin(_set), std::end(_set), to);
            if (i != std::end(_set)) {
                delete *i;
                _set.erase(i);
            }
            //if (_set.empty()) {
            //    // remove from scheduler timeout list
            //    cproc->sched().remove_timeout_task(this);
            //}
        }

        timeout *front() const { return _set.front(); }

        void pop_front() { _set.pop_front(); }

        bool empty() const { return _set.empty(); }
        size_t size() const { return _set.size(); }
    };

private:
    // when a task exits, its linked tasks are canceled
    //std::vector<std::shared_ptr<task>> _links;
    timeout_set _timeouts;
    uint64_t _id;
    uint64_t _cancel_points = 0;
    runtime *_runtime; // TODO: scheduler
    std::atomic<state> _state;

private:
    static uint64_t next_id();
    time_point first_timeout() const {
        // TODO: assert not empty
        return _timeouts.front()->when;
    }

    task() : coroutine(), _id{next_id()} {}
public:
    //! create a new coroutine
    template<class Function, class... Args> 
    explicit task(Function &&f, Args&&... args)
    : coroutine{std::forward<Function>(f), std::forward<Args>(args)...},
    _id{next_id()}
    {
    }

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
    friend uint64_t this_task::get_id();
    friend void this_task::yield();

    void yield();
    void ready();

    template <class Duration>
        timeout *set_timeout(const std::chrono::time_point<clock, Duration>& sleep_time) {
            return _timeouts.insert(sleep_time);
        }
};

class runtime {
public:
    typedef std::chrono::steady_clock clock;
    typedef std::chrono::time_point<clock> time_point;
    typedef std::shared_ptr<task> shared_task;
private:
    static __thread runtime *_runtime;

    friend class task::cancellation_point;
    friend void task::yield();
    friend void task::ready();
    friend uint64_t this_task::get_id();
    friend void this_task::yield();
    template<class Rep, class Period>
        friend void this_task::sleep_for(std::chrono::duration<Rep, Period> sleep_duration);
    template <class Clock, class Duration>
        friend void this_task::sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time);

    static task *current_task();
private:
    struct timeout_task_set {
    public:
        typedef std::vector<task *> container;
        typedef container::iterator iterator;
    private:
        std::vector<task *> _set;
        struct order {
            bool operator ()(const task *a, const task *b) const {
                return (a->first_timeout() < b->first_timeout());
            }
        };
    public:
        iterator begin() { return std::begin(_set); }
        iterator end() { return std::end(_set); }

        task *front() const { return _set.front(); }
        bool empty() const { return _set.empty(); }
        size_t size() const { return _set.size(); }

        void insert(task *t) {
            auto i = std::lower_bound(std::begin(_set), std::end(_set), t, order());
            _set.insert(i, t);
        }

        void remove(task *t) {
            //DCHECK(t->timeouts.empty());
            auto i = std::remove(std::begin(_set), std::end(_set), t);
            _set.erase(i, std::end(_set));
        }

        iterator erase(iterator first, iterator last) {
            return _set.erase(first, last);
        }
    };
private:
    task _task;
    std::vector<shared_task> _alltasks;
    std::deque<task *> _readyq;
    //! current time cached in a few places through the event loop
    time_point _now;
    llqueue<task *> _dirtyq;
    timeout_task_set _timeout_tasks;
    std::mutex _mutex;
    std::condition_variable _cv;

    const time_point &update_cached_time() {
        _now = clock::now();
        return _now;
    }

    template <class Duration>
        static void sleep_until(const std::chrono::time_point<clock, Duration>& sleep_time) {
            task *t = current_task();
            t->_state = task::state::asleep;
            t->set_timeout(sleep_time);
            _runtime->_timeout_tasks.insert(t);
            task::cancellation_point cancelable;
            t->yield();
        }

    void ready(task *t);
public:
    runtime() {
        // TODO: reenable this when our libstdc++ is fixed
        //static_assert(clock::is_steady, "clock not steady");
        _runtime = this;
        update_cached_time();
    }

    //! is this the main thread?
    static bool is_main_thread() noexcept {
        return getpid() == syscall(SYS_gettid);
    }

    //! spawn a new task in the current thread
    template<class Function, class... Args> 
    static std::shared_ptr<task> spawn(Function &&f, Args&&... args) {
        auto t = std::make_shared<task>(std::forward<Function>(f),
                std::forward<Args>(args)...);
        runtime *self = _runtime;
        t->_runtime = self;
        self->_alltasks.push_back(t);
        self->_readyq.push_back(t.get());
        return t;
    }

    static time_point now() { return _runtime->_now; }

    void operator() ();
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

// TODO: sleep_for, sleep_until

//! sleep current task for milliseconds
void sleep(uint64_t ms);

#if 0
//! suspend task waiting for io on pollfds
int poll(pollfd *fds, nfds_t nfds, uint64_t ms=0);

//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, uint64_t ms=0);
#endif


} // end namespace this_task

struct proc;


//typedef std::deque<task *> tasklist;
//typedef std::deque<proc *> proclist;

#if 0
//! get a dump of all task names and state for the current proc
std::string taskdump();
//! write task dump to FILE stream
void taskdumpf(FILE *of = stderr);

//! spawn a new thread with a task scheduler
uint64_t procspawn(const std::function<void ()> &f, size_t stacksize=default_stacksize);
//! cancel all non-system tasks and exit procmain
void procshutdown();

//! return cached time from event loop, not precise
const std::chrono::time_point<std::chrono::steady_clock> &procnow();

//! main entry point for tasks
struct procmain {
private:
    proc *p;
public:
    explicit procmain(task *t = nullptr);
    ~procmain();

    int main(int argc=0, char *argv[]=nullptr);
};
#endif

// inherit from task_interrupted so lock/rendez/poll canceling
// doesn't need to be duplicated
struct deadline_reached : task_interrupted {};

//! schedule a deadline to interrupt task with
//! deadline_reached after milliseconds
class deadline {
private:
    void *timeout_id;
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

} // end namespace task2

} // end namespace ten

#endif // LIBTEN_TASK_HH
