#include <memory>
#include "task_impl.hh"
#include "thread_context.hh"

namespace ten {

static void thread_entry(std::shared_ptr<task::pimpl> t) {
    procmain scope;
    this_ctx->scheduler.attach_task(t);
    t->ready(true);
    scope.main();
}

uint64_t procspawn(const std::function<void ()> &f, size_t stacksize) {
    std::shared_ptr<task::pimpl> t = std::make_shared<task::pimpl>(f, stacksize);
    uint64_t tid = t->id;
    std::thread procthread{thread_entry, std::move(t)};
    procthread.detach();
    // XXX: task could be freed at this point
    return tid;
}

void procshutdown() {
    this_ctx->scheduler.shutdown();
}


procmain::procmain() {
    this_ctx->scheduler.current_task(); // cause runtime_init to be run
}

int procmain::main(int argc, char *argv[]) {
    DVLOG(5) << "thread id: " << std::this_thread::get_id();
    this_ctx->scheduler.loop();
    DVLOG(5) << "thread done: " << std::this_thread::get_id();
    return EXIT_SUCCESS;
}

const time_point<steady_clock> &procnow() {
    return this_ctx->scheduler.cached_time();
}

} // end namespace ten
