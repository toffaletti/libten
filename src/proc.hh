#ifndef LIBTEN_PROC_HH
#define LIBTEN_PROC_HH

#include "scheduler.hh"

namespace ten {

extern ptr<proc> this_proc();

class proc {
private:
    friend class scheduler;
    scheduler _scheduler;
private:
    friend class task;
    friend void this_task::yield();

    void ready(ptr<task::pimpl> t, bool front=false) {
        _scheduler.ready(t, front);
    }

    void ready_for_io(ptr<task::pimpl> t) {
        _scheduler.ready_for_io(t);
    }

    void unsafe_ready(ptr<task::pimpl> t) {
        _scheduler.unsafe_ready(t);
    }
public:
    explicit proc(bool main) : _scheduler(main) {}

    proc(const proc &) = delete;
    proc &operator =(const proc &) = delete;

    void schedule() {
        _scheduler.schedule();
    }

    context &sched_context() { return _scheduler.ctx; }

    io_scheduler &sched() {
        return _scheduler.sched();
    }

    ptr<task::pimpl> current_task() const {
        return _scheduler.ctask;
    }

    bool is_canceled() const {
        return _scheduler.canceled;
    }

    void cancel() {
        _scheduler.canceled = true;
        _scheduler.wakeup();
    }

    bool is_dirty() {
        return !_scheduler.dirtyq.empty();
    }

    bool is_ready() const {
        return !_scheduler.runqueue.empty();
    }

    std::shared_ptr<proc_waker> get_waker() {
        return _scheduler._waker;
    }

    const proc_time_t & update_cached_time() {
        return _scheduler.update_cached_time();
    }

    const proc_time_t &cached_time() const {
        return _scheduler.now;
    }

    void dump_tasks(std::ostream &out) const {
        _scheduler.dump_tasks(out);
    }

    //! cancel all tasks in this proc
    void shutdown() {
        _scheduler.shutdown();
    }

    bool cancel_task_by_id(uint64_t id) {
        return _scheduler.cancel_task_by_id(id);
    }
    //! mark current task as system task
    void mark_system_task() {
        _scheduler.mark_system_task();
    }

    void addtaskinproc(std::shared_ptr<task::pimpl> t) {
        _scheduler.attach_task(t);
    }

    void deltaskinproc(ptr<task::pimpl> t) {
        _scheduler.remove_task(t);
    }

    static void add(ptr<proc> p);
    static void del(ptr<proc> p);

    static void thread_entry(std::shared_ptr<task::pimpl> t);

};

} // end namespace ten

#endif // LIBTEN_PROC_HH
