#ifndef TEN_KERNEL_HH
#define TEN_KERNEL_HH

#include <chrono>

namespace ten {
namespace kernel {

using clock = std::chrono::steady_clock;
using time_point = std::chrono::time_point<clock>;

//! return cached time from event loop, not precise
time_point now();

//! is this the main thread?
bool is_main_thread();

//! number of available cpus
size_t cpu_count();

//! perform setup
void boot();

//! perform clean shutdown
void shutdown();

//! this is only a tribute
bool is_computer_on();
bool is_computer_on_fire();

} // kernel
} // ten

#endif
