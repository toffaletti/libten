#ifndef TEN_THREAD_CONTEXT_HH
#define TEN_THREAD_CONTEXT_HH
#include "ten/thread_local.hh"
#include "scheduler.hh"

namespace ten {

//! per-thread context for runtime
struct thread_context {
    ten::scheduler scheduler;

    thread_context();
    ~thread_context();

    void cancel_all();

    static void dump_all();
    static size_t count();
};

struct runtime_tag {};
extern thread_cached<runtime_tag, thread_context> this_ctx;

} // ten

#endif
