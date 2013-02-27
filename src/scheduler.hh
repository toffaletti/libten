#ifndef LIBTEN_SCHEDULER_HH
#define LIBTEN_SCHEDULER_HH

#include <condition_variable>
#include "ten/descriptors.hh"
#include "ten/llqueue.hh"
#include "kernel_private.hh"
#include "alarm.hh"
#include "io.hh"

namespace ten {

// TODO: api to register at-proc-exit cleanup functions
// this can be used to free io, or other per-proc
// resources like dns resolving threads, etc.

class scheduler {
    friend kernel::time_point kernel::now();
    friend ptr<task::pimpl> kernel::current_task();
public:
    typedef ten::alarm_clock<ptr<task::pimpl>, kernel::clock> alarm_clock;

    //! return an armed alarm, used by sleep_until, deadline, and io timeouts
    template <class ...Args>
        alarm_clock::scoped_alarm arm_alarm(Args&& ...args) {
            return std::move(alarm_clock::scoped_alarm{_alarms, std::forward<Args>(args)...});
        }
private:
    //! task representing OS allocated stack for this thread
    std::shared_ptr<task::pimpl> _main_task;
    //! ptr to the currently running task
    ptr<task::pimpl> _current_task;
    //! queue of tasks ready to run
    std::deque<ptr<task::pimpl>> _readyq;
    //! other threads use this to add tasks to ready queue
    llqueue<ptr<task::pimpl>> _dirtyq;
    //! epoll io
    optional<io> _io;
    //! all tasks known to this scheduler
    std::deque<std::shared_ptr<task::pimpl>> _alltasks;
    //! tasks to be garbage collected in the next scheduler iteration
    std::deque<std::shared_ptr<task::pimpl>> _gctasks;
    //! current time cached in a few places through the event loop
    kernel::time_point _now;
    //! tasks with pending timeouts
    alarm_clock _alarms;

    //! lock to protect condition
    std::mutex _mutex;
    //! cond used to wake up when runqueue is empty and no epoll
    std::condition_variable _cv;

    //! true when canceled
    std::atomic<bool> _canceled;
    //! scheduler is trying to shutdown
    bool _shutdown_sequence_initiated = false;
    //! main task is looping in wait_for_all
    bool _looping = false;
private:
    void check_canceled();
    void check_dirty_queue();
    void check_timeout_tasks();

    const kernel::time_point & update_cached_time() {
        _now = kernel::clock::now();
        return _now;
    }

public:
    scheduler();
    ~scheduler();

    // not movable or copyable
    scheduler(const scheduler &) = delete;
    scheduler(scheduler &&) = delete;
    scheduler &operator= (const scheduler &) = delete;
    scheduler &operator= (scheduler &&) = delete;

    //! cancel all tasks in this scheduler
    void shutdown();

    //! get io manager for this scheduler
    io &get_io();

    //! wait for all tasks to exit
    // will only work if called from main task
    // see hack in ::schedule()
    void wait_for_all();
    //! run one iteration of the scheduler
    void schedule();

    void attach_task(std::shared_ptr<task::pimpl> t);
    void remove_task(ptr<task::pimpl> t);

    void wakeup();
    void wait(std::unique_lock <std::mutex> &lock, optional<kernel::time_point> when);

    bool cancel_task_by_id(uint64_t id);

    void cancel() {
        _canceled = true;
        wakeup();
    }

    void dump() const;
private:
    friend class task;
    friend void this_task::yield();
    void ready(ptr<task::pimpl> t, bool front=false);
    void ready_for_io(ptr<task::pimpl> t);
    void unsafe_ready(ptr<task::pimpl> t);
};

} // ten

#endif

