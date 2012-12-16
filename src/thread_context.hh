#ifndef TEN_RUNTIME_CONTEXT_HH
#define TEN_RUNTIME_CONTET_HH
#include "ten/task/runtime.hh"
#include "ten/task/task_pimpl.hh"
#include "ten/thread_local.hh"
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>

namespace ten {
//! per-thread context for runtime
struct thread_context {
    task_pimpl _task;
    task_pimpl *_current_task = nullptr;
    std::vector<runtime::shared_task> _alltasks;
    std::vector<runtime::shared_task> _gctasks;
    std::deque<task_pimpl *> _readyq;
    //! current time cached in a few places through the event loop
    runtime::time_point _now;
    llqueue<task_pimpl *> _dirtyq;
    runtime::alarm_clock _alarms;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _canceled;
    bool _tasks_canceled = false;

    thread_context();
    ~thread_context();

    const runtime::time_point &update_cached_time() {
         _now = runtime::clock::now();
         return _now;
    }

    void ready(task_pimpl *t);
    void check_dirty_queue();
    void check_timeout_tasks();
    void remove_task(task_pimpl *t);
    int dump();
    void attach(runtime::shared_task t);
    void schedule();
    void cancel();
};

} // ten

#endif
