#include <memory>
#include "task_impl.hh"
#include "thread_context.hh"

namespace ten {

void procspawn(const std::function<void ()> &f, size_t stacksize) {
    // XXX: stack size is ignored now
    std::thread procthread{task::spawn_thread([=] {
        f();
    })};
    procthread.detach();
}

void procshutdown() {
    kernel::shutdown();
}

procmain::procmain() {
    kernel::current_task(); // causes kernel::boot to be called
}

int procmain::main(int argc, char *argv[]) {
    DVLOG(5) << "thread id: " << std::this_thread::get_id();
    this_ctx->scheduler.wait_for_all(1);
    DVLOG(5) << "thread done: " << std::this_thread::get_id();
    return EXIT_SUCCESS;
}

time_point<steady_clock> procnow() {
    return kernel::now();
}

} // end namespace ten
