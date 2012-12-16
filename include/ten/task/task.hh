#ifndef LIBTEN_TASK_HH
#define LIBTEN_TASK_HH

#include <functional>
#include <memory>
#include <cstdint>

namespace ten {
//! exception to unwind stack on taskcancel
struct task_interrupted {};

// forward decl
class task_pimpl;

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
