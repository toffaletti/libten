#ifndef TEN_KERNEL_HH
#define TEN_KERNEL_HH

#include "ten/ptr.hh"
#include <chrono>

namespace ten {

class kernel {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration   = clock::duration;

    //! return cached time from event loop, not precise
    static time_point now();

    //! is this the main thread?
    static bool is_main_thread();

    //! number of available cpus
    static size_t cpu_count();

    //! perform setup
    static void boot();

    //! wait for all tasks
    static void wait_for_tasks();

    //! perform clean shutdown
    static void shutdown();

    //! this is only a tribute
    static int32_t is_computer_on();
    static double is_computer_on_fire();

    kernel();
    ~kernel(); 
};

} // ten

#endif
