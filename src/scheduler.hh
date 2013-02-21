#ifndef LIBTEN_SCHEDULER_HH
#define LIBTEN_SCHEDULER_HH

#include <condition_variable>
#include "ten/descriptors.hh"
#include "ten/llqueue.hh"
#include "task_impl.hh"
#include "alarm.hh"
#include "io.hh"

namespace ten {

// TODO: api to register at-proc-exit cleanup functions
// this can be used to free io, or other per-proc
// resources like dns resolving threads, etc.

class scheduler {
    typedef ten::alarm_clock<ptr<task::pimpl>, proc_clock_t> alarm_clock;
    friend class proc;
    friend struct io;
    friend void this_task::sleep_until(const proc_time_t& sleep_time);
    friend class deadline;
    friend struct deadline_pimpl;
private:
    std::shared_ptr<task::pimpl> _main_task;
    //ptr<task::pimpl> _current_task;
    ptr<task::pimpl> ctask;
    std::deque<ptr<task::pimpl>> _readyq;
    //! other threads use this to add tasks to runqueue
    llqueue<ptr<task::pimpl>> _dirtyq;
    //! epoll io
    optional<io> _io;
    //! all tasks known to this scheduler
    std::deque<std::shared_ptr<task::pimpl>> _alltasks;
    //! tasks to be garbage collected in the next scheduler iteration
    std::deque<std::shared_ptr<task::pimpl>> _gctasks;
    //! tasks in this proc
    uint64_t _taskcount; // TODO: can probably remove this and use _alltasks.size
    //! current time cached in a few places through the event loop
    proc_time_t _now;
    //! tasks with pending timeouts
    alarm_clock _alarms;

    //! lock to protect condition
    std::mutex _mutex;
    //! cond used to wake up when runqueue is empty and no epoll
    std::condition_variable _cv;

    //! true when canceled
    std::atomic<bool> _canceled;
    bool _shutdown_sequence_initiated = false;
    bool _looping = false;
private:
    void check_canceled();
    void check_dirty_queue();
    void check_timeout_tasks();
public:
    scheduler();
    ~scheduler();

    //! cancel all tasks in this scheduler
    void shutdown();
    io &get_io();

    //! wait for all tasks to exit
    // will only work if called from main task
    // see hack in ::schedule()
    void wait_for_all();
    //! run one iteration of the scheduler
    void schedule();

    const proc_time_t & update_cached_time() {
        _now = proc_clock_t::now();
        return _now;
    }

    const proc_time_t &cached_time() const {
        return _now;
    }

    void attach_task(std::shared_ptr<task::pimpl> t);
    void remove_task(ptr<task::pimpl> t);

    void wakeup();
    void wait(std::unique_lock <std::mutex> &lock, optional<proc_time_t> when);

    bool cancel_task_by_id(uint64_t id);

    ptr<task::pimpl> current_task() const {
        return ctask;
    }

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

