#include "thread_context.hh"
#include "ten/synchronized.hh"

namespace ten {

// ** begin global ordering **
// XXX: order of these globals is important
// threads and cache are potentially used by
// ~thread_context because of scheduler
// so they must be declared *before* this_ctx
extern bool glog_inited;

namespace {
    struct stoplog_t {
        ~stoplog_t() { if (glog_inited) ShutdownGoogleLogging(); }
    } stoplog;

    using tvec_t = std::vector<ptr<thread_context>>;
    static synchronized<tvec_t> threads;

    int dummy = stack_allocator::initialize();
} // anon namespace

inotify_fd resolv_conf_watch_fd{IN_NONBLOCK};
__thread thread_context *this_ctx = nullptr;
// *** end global ordering **

thread_context::thread_context() {
    CHECK(this_ctx == nullptr);
    this_ctx = this;
    const ptr<thread_context> me(this);
    threads([me](tvec_t &tvec) {
        tvec.push_back(me);
    });
}

thread_context::~thread_context() {
    scheduler.wait_for_all();
    const ptr<thread_context> me(this);
    threads([me](tvec_t &tvec) {
        auto i = find(begin(tvec), end(tvec), me);
        if (i == end(tvec))
            LOG(FATAL) << "BUG: thread " << (void*)me.get() << " escaped the thread list";
        tvec.erase(i);
    });
    this_ctx = nullptr;
}

size_t thread_context::count() {
    return threads([](const tvec_t &tvec) {
        return tvec.size();
    });
}

void thread_context::cancel_all() {
    threads([](const tvec_t &tvec) {
        for (auto &ctx : tvec) {
            ctx->scheduler.cancel();
        }
    });
}

void thread_context::dump_all() {
    threads([](const tvec_t &tvec) {
        for (auto &ctx : tvec) {
            ctx->scheduler.dump();
        }
    });
}

} // ten
