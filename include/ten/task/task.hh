#ifndef LIBTEN_TASK_TASK_HH
#define LIBTEN_TASK_TASK_HH

#include "ten/task/kernel.hh"
#include "ten/optional.hh"
#include <memory>
#include <chrono>
#include <functional>

namespace ten {

//! exception to unwind stack on taskcancel
struct task_interrupted {};

typedef optional<std::chrono::milliseconds> optional_timeout;

namespace this_task {

//! id of the current task
uint64_t get_id();

//! allow other tasks to run
void yield();

//! sleep current task until time is reached
void sleep_until(const kernel::time_point & sleep_time);

//! sleep current task until duration has passed
template <class Rep, class Period>
    void sleep_for(std::chrono::duration<Rep, Period> sleep_duration) {
        sleep_until(kernel::now() + sleep_duration);
    }

} // end namespace this_task

//! cooperatively scheduled light-weight threads of execution
class task {
public:
    class impl;
private:
    std::shared_ptr<impl> _impl;

    explicit task(const std::function<void ()> &f);
public:
    task() {}
    task(const task &) = delete;
    task &operator=(const task &) = delete;
    task(task &&) = default;
    task &operator=(task &&) = default;

    //! spawn a new task in the current thread
    template<class Function, class... Args> 
        static task spawn(Function &&f, Args&&... args) {
            return task{std::bind(f, std::forward<Args>(args)...)};
        }

    //! id of this task
    uint64_t get_id() const;

    //! set stack size that tasks will use when spawning
    static void set_default_stacksize(size_t stacksize);
};

} // end namespace ten



#endif
