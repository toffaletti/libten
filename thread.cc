#include "thread.hh"

#include <iostream>

namespace detail {
__thread thread *thread_ = NULL;
}

std::ostream &operator << (std::ostream &o, coro_list &l) {
    o << "[";
    for (coro_list::iterator i=l.begin(); i!=l.end(); ++i) {
        o << *i << ",";
    }
    o << "]";
    return o;
}

void thread::schedule() {
    for (;;) {
        mutex::scoped_lock l(mut);
        while (!runq.empty()) {
            std::cout << "runq: " << runq << "\n";
            coro_list::iterator i = runq.begin();
            while (i != runq.end()) {
                l.unlock();
                std::cout << "swapping: " << *i << "\n";
                bool exiting = coroutine::swap(&scheduler, *i);
                l.lock();
                if (exiting) {
                    std::cout << "deleting: " << *i << "\n";
                    delete *i;
                    i = runq.erase(i);
                } else {
                    ++i;
                }
            }
        }
        //sleep();
        return;
    }
}

void *thread::start(void *arg) {
    using namespace detail;
    thread_ = (thread *)arg;
    detail::thread_->schedule();
    // TODO: if detatched, free memory here
    if (detail::thread_->detached) {
        delete detail::thread_;
    }
    return NULL;
}

coroutine *coroutine::self() {
    return thread::self().get_coro();
}

bool coroutine::swap(coroutine *from, coroutine *to) {
    // TODO: wrong place for this code. put in scheduler
    thread::self().set_coro(to);
    std::cout << "swapping from " <<
        &(from->context) << " to " << &(to->context) <<
        " in thread: " << thread::self().id() << "\n";
    int r = swapcontext(&from->context, &to->context);
    return to->exiting;
}

coroutine *coroutine::spawn(const func_t &f) {
    coroutine *c = new coroutine(f);
    detail::thread_->add_to_runqueue(c);
    return c;
}

void coroutine::switch_() {
    coroutine::swap(this, &detail::thread_->scheduler);
}

void coroutine::yield() {
    std::cout << "coro " << coroutine::self() << " yielding in thread: " << thread::self().id() << "\n";
    coroutine::self()->switch_();
}

void coroutine::start(coroutine *c) {
    try {
            c->f();
    } catch(...) {
    }
    c->exiting = true;
    c->switch_();
}
