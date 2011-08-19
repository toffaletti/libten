#ifndef RUNNER_HH
#define RUNNER_HH

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include "descriptors.hh"
#include "thread.hh"
#include <list>
#include <poll.h>

namespace fw {

#define DEFAULT_STACK_SIZE (8*1024)

class task;
class scheduler;

//! scheduler for tasks

//! runs tasks in an OS-thread
//! uses epoll for io events and timeouts
class runner {
public:
    typedef boost::function<void ()> proc;
    typedef void (*unspecified_bool_type);
    // thrown when runner has no tasks for thread_timeout_ms
    // runner will then be removed from runners list
    struct timeout_exit : std::exception {};

    operator unspecified_bool_type() const;
    bool operator == (const runner &r) const;

    //! \return the thread this runner is using
    thread get_thread();

    //! init the main runner
    static void init();

    //! run the main scheduler
    static int main();

    //! spawn a new runner with a task that will execute
    //
    //! if there is already a runner for every cpu then
    //! this will reuse an existing runner unless force is true
    static runner spawn(const proc &f,
        bool force=false,
        size_t stack_size=DEFAULT_STACK_SIZE);

    //! spawn a new runner for this task or reuse an empty one
    static runner spawn(task &t);

    //! \return the runner for this thread
    static runner self();

    //! schedule all tasks in this runner
    //! will block until all tasks exit
    void schedule();

    //! number of processors
    static unsigned long ncpu();

    //! number of runners
    static unsigned long count();

    //! amount of time a runner with no tasks will wait for work before exiting
    static void set_thread_timeout(unsigned int ms);

    scheduler &get_scheduler();

    //! add task to run queue.
    //! will wakeup runner if it was sleeping.
    // NOTE: used internally by scheduler for migrate
    void add_to_runqueue(task &t);

private: /* task interface */
    friend class task;

    void set_task(task &t);
    task get_task();

    //! add fds to this runners epoll fd
    //! \param t task to wake up for fd events
    //! \param fds pointer to array of pollfd structs
    //! \param nfds number of pollfd structs in array
    void add_pollfds(task &t, pollfd *fds, nfds_t nfds);

    //! remove fds from epoll fd
    int remove_pollfds(pollfd *fds, nfds_t nfds);

    //! add task to wait list.
    //! used for io and timeouts
    void add_waiter(task &t);
    //! remove task from wait list
    void remove_waiter(task &t);

    static void swap_to_scheduler();

private: /* internal */
    struct impl;
    typedef std::list<runner> list;
    typedef boost::shared_ptr<impl> shared_impl;
    shared_impl m;

    static unsigned int thread_timeout_ms;
    static thread main_thread_;
    static unsigned long ncpu_;
    static __thread runner::impl *impl_;
    static list *runners;
    static mutex *tmutex;
    static void append_to_list(runner &r, mutex::scoped_lock &);
    static void remove_from_list(runner &);
    static void wakeup_all_runners();

    runner();
    runner(task &t);
    runner(const shared_impl &m_);

    static void *start(void *arg);
};

} // end namespace fw

#endif // RUNNER_HH
