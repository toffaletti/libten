#ifndef TEN_THREAD_CONTEXT_HH
#define TEN_THREAD_CONTEXT_HH
#include "ten/thread_local.hh"
#include "scheduler.hh"
#include <ares.h>

namespace ten {

//! per-thread context for runtime
struct thread_context {
    // XXX: the downside to having one ares_channel per thread
    // is that if resolv.conf ever changes we won't see the changes
    // because c-ares only reads it when ares_init is called.
    // we address this by using an inotify watch on /etc/resolv.conf
    // and reset the shared_ptr on changes

    // ensure this is freed *after* scheduler
    // so all tasks will have exited
    std::shared_ptr<ares_channeldata> dns_channel;

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
