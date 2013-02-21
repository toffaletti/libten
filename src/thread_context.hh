#ifndef TEN_THREAD_CONTEXT_HH
#define TEN_THREAD_CONTEXT_HH
#include "ten/thread_local.hh"
#include "scheduler.hh"
#include "kernel_private.hh"

#ifdef HAS_CARES
    #include <ares.h>
#endif // HAS_CARES

namespace ten {

//! per-thread context for runtime
struct thread_context {
#ifdef HAS_CARES
    // XXX: the downside to having one channel per thread
    // is that if resolv.conf ever changes we won't see the changes
    // because c-ares only reads it when ares_init is called.
    // TODO: fix this by setting a watch on /etc/resolv.conf
    // and recreate the channel when no dns queries are pending

    // ensure this is freed *after* scheduler
    // so all tasks will have exited
    std::shared_ptr<ares_channeldata> dns_channel;
#endif // HAS_CARES
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
