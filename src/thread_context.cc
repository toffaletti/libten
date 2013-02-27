#include "thread_context.hh"
#include "kernel_private.hh"

namespace ten {

// ** begin global ordering **
// XXX: order of these globals is important
// threads and cache are potentially used by
// ~thread_context because of scheduler
// so they must be declared *before* this_ctx
namespace {
    static std::mutex threads_mutex;
    static std::vector<ptr<thread_context>> threads;
} // namespace
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
    task::set_default_stacksize(default_stacksize);
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
        if (ptr<thread_context>{this} == ctx) continue;
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
