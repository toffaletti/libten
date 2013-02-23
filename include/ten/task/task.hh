#ifndef LIBTEN_TASK_TASK_HH
#define LIBTEN_TASK_TASK_HH

namespace ten {

//! exception to unwind stack on taskcancel
struct task_interrupted {};

typedef optional<std::chrono::milliseconds> optional_timeout;

namespace this_task {

//! id of the current task
uint64_t get_id();

//! allow other tasks to run
void yield();

void sleep_until(const kernel::time_point & sleep_time);

template <class Rep, class Period>
    void sleep_for(std::chrono::duration<Rep, Period> sleep_duration) {
        sleep_until(kernel::now() + sleep_duration);
    }

} // end namespace this_task

class task {
public:
    class pimpl;
private:
    std::shared_ptr<pimpl> _pimpl;

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
            return task{std::bind(f, args...)};
        }

    //! id of this task
    uint64_t get_id() const;

public:
    static void set_default_stacksize(size_t stacksize);
};

} // end namespace ten



#endif
