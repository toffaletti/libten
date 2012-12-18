#ifndef TEN_RUNTIME_CONTEXT_HH
#define TEN_RUNTIME_CONTET_HH
#include "alarm.hh"
#include "task_pimpl.hh"
#include "ten/task/runtime.hh"
#include "ten/thread_local.hh"
#include "scheduler.hh"

namespace ten {
//! per-thread context for runtime
struct thread_context {
    ten::scheduler scheduler;

    thread_context();
    ~thread_context();
};

} // ten

#endif
