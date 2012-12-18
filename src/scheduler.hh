#ifndef TEN_TASK_SCHEDULER_HH
#define TEN_TASK_SCHEDULER_HH

#include "ten/llqueue.hh"
#include "ten/task/runtime.hh"
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>

namespace ten {

class runtime;
class deadline;
class io;

class scheduler {
public:
    typedef ten::alarm_clock<task_pimpl *, runtime::clock> alarm_clock;
    // TODO: make not friend when alarms are decoupled
    friend class runtime;
    friend class deadline;
    friend class io;
private:
    task_pimpl _task;
    //! current time cached in a few places through the event loop
    runtime::time_point _now;
    alarm_clock _alarms;


    task_pimpl *_current_task = nullptr;
    std::vector<runtime::shared_task> _alltasks;
    std::vector<runtime::shared_task> _gctasks;
    std::deque<task_pimpl *> _readyq;
    llqueue<task_pimpl *> _dirtyq;

    std::mutex _mutex;
    std::condition_variable _cv;

    std::atomic<bool> _canceled;
    bool _tasks_canceled = false;

    void check_dirty_queue();
    void check_timeout_tasks();

    const runtime::time_point &update_cached_time() {
         _now = runtime::clock::now();
         return _now;
    }
public:
    void ready(task_pimpl *t);
    void ready_for_io(task_pimpl *t);

    void remove_task(task_pimpl *t);
    int dump();
    void attach(runtime::shared_task t);
    void schedule();
    void cancel();

    void unsafe_ready(task_pimpl *t) {
        _readyq.push_back(t);
    }

    runtime::shared_task task_with_id(uint64_t id) {
        for (auto t : _alltasks) {
            if (t->_id == id) return t;
        }
        return nullptr;
    }

    scheduler();
    ~scheduler();
};


} // ten

#endif
