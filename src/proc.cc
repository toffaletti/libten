#include "proc.hh"
#include "io.hh"

namespace ten {

std::mutex procsmutex;
std::deque<ptr<proc>> procs;

void proc_waker::wait() {
    std::unique_lock<std::mutex> lk{mutex};
    while (!this_ctx->scheduler.is_ready()
            && !this_ctx->scheduler.is_canceled()
            && !this_ctx->scheduler.is_dirty())
    {
        asleep = true;
        cond.wait(lk);
    }
    asleep = false;
}

void proc::thread_entry(std::shared_ptr<task::pimpl> t) {
    procmain scope{t};
    t->ready(true);
    scope.main();
}

void proc::add(ptr<proc> p) {
    std::lock_guard<std::mutex> lk{procsmutex};
    procs.push_back(p);
}

void proc::del(ptr<proc> p) {
    std::lock_guard<std::mutex> lk{procsmutex};
    auto i = std::find(procs.begin(), procs.end(), p);
    procs.erase(i);
}

uint64_t procspawn(const std::function<void ()> &f, size_t stacksize) {
    std::shared_ptr<task::pimpl> t = std::make_shared<task::pimpl>(f, stacksize);
    uint64_t tid = t->id;
    std::thread procthread{proc::thread_entry, std::move(t)};
    procthread.detach();
    // XXX: task could be freed at this point
    return tid;
}

void procshutdown() {
    this_ctx->scheduler.shutdown();
}

procmain::procmain(std::shared_ptr<task::pimpl> t) {
    p.reset(new proc());
    proc::add(p);
    if (t) {
        this_ctx->scheduler.attach_task(t);
    }
}

procmain::~procmain() {
    proc::del(p);
    delete p.get();
}

int procmain::main(int argc, char *argv[]) {
    DVLOG(5) << "proc: " << p.get() << " thread id: " << std::this_thread::get_id();
    this_ctx->scheduler.schedule();
    DVLOG(5) << "proc done: " << std::this_thread::get_id() << " " << p.get();
    return EXIT_SUCCESS;
}

const time_point<steady_clock> &procnow() {
    return this_ctx->scheduler.cached_time();
}

} // end namespace ten
