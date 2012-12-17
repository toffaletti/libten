#ifndef LIBTEN_TASK_HH
#define LIBTEN_TASK_HH

#include <functional>
#include <memory>
#include <chrono>
#include <cstdint>
#include "ten/task/runtime.hh"

namespace ten {

//! exception to unwind stack on taskcancel
struct task_interrupted {};

// forward decl
class task_pimpl;

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

} // end namespace this_task

class task {
private:
    std::shared_ptr<task_pimpl> _pimpl;

    explicit task(std::function<void ()> f);
public:
    task() {}
    task(const task &) = delete;
    task(task &&other);
    task &operator=(task &&other) noexcept;

    //! spawn a new task in the current thread
    template<class Function, class... Args> 
    static task spawn(Function &&f, Args&&... args) {
        return task{std::bind(f, args...)};
    }

    //! id of this task
    uint64_t get_id() const;
    //! cancel this task
    void cancel();
    //! make the task a system task
    void detach();
    //! join task
    void join();
};

} // end namespace ten

#endif // LIBTEN_TASK_HH
