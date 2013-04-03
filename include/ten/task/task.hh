#ifndef LIBTEN_TASK_TASK_HH
#define LIBTEN_TASK_TASK_HH

#include "ten/task/kernel.hh"
#include "ten/optional.hh"
#include "ten/error.hh"
#include <memory>
#include <chrono>
#include <functional>
#include <thread>
#include <sys/time.h>

namespace ten {

//! exception to unwind stack on task cancel
struct task_interrupted {};

typedef optional<std::chrono::milliseconds> optional_timeout;

template <class D>
inline D timeval_duration(timeval tv) {
    using namespace std::chrono;
    return   duration_cast<D>(seconds{tv.tv_sec})
           + duration_cast<D>(microseconds{tv.tv_usec});
}

//! dummy type to smooth API change
enum nostacksize_t { nostacksize };

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

    //! enter the task system
    static int introduce(std::function<void()> f);

    //! call from main() to setup task system and do boilerplate exception handling
    template <class Func>
        static int main(Func &&f, optional<size_t> stacksize=nullopt) {
            kernel the_kernel(stacksize);
            return introduce(std::forward<Func>(f));
        }

    task() {}
    task(const task &) = delete;
    task &operator=(const task &) = delete;
    task(task &&) = default;
    task &operator=(task &&) = default;

    //! spawn a new task in the current thread
    template<class Function> 
        static task spawn(Function &&f) {
            return task{f};
        }

    //! spawn a new task in a new thread
    // returns a joinable std::thread
    template<class Function> 
        static std::thread spawn_thread(Function &&f) {
            return std::thread([f] {
                task::introduce(f);
            });
        }

    //! id of the task
    uint64_t get_id() const;

    //! cancel the task
    void cancel();

    //! is task joinable
    bool joinable() noexcept;

    //! join the task or throw system_error
    void join();

private:
    std::shared_ptr<impl> _impl;

    explicit task(std::function<void ()> f);

    //! task entry boilerplate exception handling
    static int entry(std::function<void ()> f);
};

} // end namespace ten



#endif
