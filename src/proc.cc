#include <memory>
#include "task_impl.hh"
#include "thread_context.hh"

namespace ten {

void procspawn(const std::function<void ()> &f, size_t stacksize) {
    // XXX: stack size is ignored now
    std::thread procthread{[=] {
        try {
            f();
        } catch (task_interrupted &e) {}
    }};
    procthread.detach();
}

void procshutdown() {
    this_ctx->scheduler.shutdown();
}


procmain::procmain() {
    this_ctx->scheduler.current_task(); // cause runtime_init to be run
}

int procmain::main(int argc, char *argv[]) {
    DVLOG(5) << "thread id: " << std::this_thread::get_id();
    this_ctx->scheduler.wait_for_all();
    DVLOG(5) << "thread done: " << std::this_thread::get_id();
    return EXIT_SUCCESS;
}

const time_point<steady_clock> &procnow() {
    return this_ctx->scheduler.cached_time();
}

} // end namespace ten
