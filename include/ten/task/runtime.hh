#ifndef TEN_RUNTIME_HH
#define TEN_RUNTIME_HH

#include <chrono>
#include <memory>
#include <deque>
#include <unistd.h>
#include <sys/syscall.h>

namespace ten {

struct task_pimpl;

typedef std::shared_ptr<task_pimpl> shared_task;
typedef std::weak_ptr<task_pimpl> weak_task;
typedef std::deque<task_pimpl *> tasklist;

class runtime {
public:
    typedef std::chrono::steady_clock clock;
    typedef std::chrono::time_point<clock> time_point;
public:
    // TODO: should be private
    static task_pimpl *current_task();
    static void attach(shared_task &t);
private:

    static int dump_all();

public:
    static void sleep_until(const time_point &sleep_time);

    //! is this the main thread?
    static bool is_main_thread() noexcept {
        return getpid() == syscall(SYS_gettid);
    }

    static time_point now();

    // compat
    static shared_task task_with_id(uint64_t id);

    static void wait_for_all();

    static void shutdown();
};

} // ten

#endif
