#include "thread_context.hh"

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

    static std::mutex threads_mutex;
    static std::vector<ptr<thread_context>> threads;
} // namespace

inotify_fd resolv_conf_watch_fd{IN_NONBLOCK};
thread_cached<stack_allocator::stack_cache, std::list<stack_allocator::stack>> stack_allocator::_cache;
thread_cached<runtime_tag, thread_context> this_ctx;
// *** end global ordering **

static void add_thread(ptr<thread_context> ctx) {
    using namespace std;
    lock_guard<mutex> lock{threads_mutex};
    threads.push_back(ctx);
}

static void remove_thread(ptr<thread_context> ctx) {
    using namespace std;
    lock_guard<mutex> lock{threads_mutex};
    auto i = find(begin(threads), end(threads), ctx);
    threads.erase(i);
}

thread_context::thread_context() {
    kernel::boot();
    add_thread(ptr<thread_context>{this});
}

thread_context::~thread_context() {
    remove_thread(ptr<thread_context>{this});
}

size_t thread_context::count() {
    using namespace std;
    lock_guard<mutex> lock{threads_mutex};
    return threads.size();
}

void thread_context::cancel_all() {
    using namespace std;
    lock_guard<mutex> lock{threads_mutex};
    for (auto &ctx : threads) {
        ctx->scheduler.cancel();
    }
}

void thread_context::dump_all() {
    using namespace std;
    lock_guard<mutex> lock{threads_mutex};
    for (auto &ctx : threads) {
        ctx->scheduler.dump();
    }
}

} // ten
