#include "thread.hh"

#include <iostream>

namespace detail {
__thread thread *thread_ = NULL;
}

thread::list thread::threads;
mutex thread::tmutex;

std::ostream &operator << (std::ostream &o, coro_list &l) {
    o << "[";
    for (coro_list::iterator i=l.begin(); i!=l.end(); ++i) {
        o << *i << ",";
    }
    o << "]";
    return o;
}

size_t thread::count() {
    mutex::scoped_lock l(thread::tmutex);
    return thread::threads.size();
}

void thread::schedule(bool loop) {
    for (;;) {
        {
            mutex::scoped_lock l(mut);
            while (!runq.empty()) {
                std::cout << "runq: " << runq << "\n";
                coroutine *c = runq.front();
                runq.pop_front();
                runq.push_back(c);
                l.unlock();
                std::cout << "swapping: " << c << "\n";
                bool exiting = coroutine::swap(&scheduler, c);
                l.lock();
                if (exiting) {
                    std::cout << "deleting: " << c << "\n";
                    delete c;
                    runq.remove(c);
                }
            }
        }
        if (loop)
            sleep();
        else
            break;
    }
}

void *thread::start(void *arg) {
    using namespace detail;
    thread_ = (thread *)arg;
    detail::thread_->append_to_list();
    try {
        detail::thread_->schedule();
    } catch (...) {}
    detail::thread_->remove_from_list();
    // TODO: if detatched, free memory here
    if (detail::thread_->detached) {
        delete detail::thread_;
    }
    return NULL;
}

coroutine *coroutine::self() {
    return thread::self()->get_coro();
}

bool coroutine::swap(coroutine *from, coroutine *to) {
    // TODO: wrong place for this code. put in scheduler
    thread::self()->set_coro(to);
    std::cout << "swapping from " <<
        &(from->context) << " to " << &(to->context) <<
        " in thread: " << thread::self()->id() << "\n";
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
    std::cout << "coro " << coroutine::self() << " yielding in thread: " << thread::self()->id() << "\n";
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

void coroutine::migrate() {
    coroutine *c = coroutine::self();
    thread *t = thread::self();
    t->delete_from_runqueue(c);
    thread::add_to_empty_runqueue(c);
    coroutine::yield();
    // will resume in other thread
}

void coroutine::migrate_to(thread *to) {
    coroutine *c = coroutine::self();
    thread *from = thread::self();
    from->delete_from_runqueue(c);
    to->add_to_runqueue(c);
    coroutine::yield();
}

void thread::add_to_empty_runqueue(coroutine *c) {
    mutex::scoped_lock l(tmutex);
    bool added = false;
    for (thread::list::iterator i=threads.begin(); i!=threads.end(); ++i) {
        std::cout << "testing thread: " << *i << "\n";
        if ((*i)->add_to_runqueue_if_asleep(c)) {
            std::cout << "added to thread: " << (*i) << "\n";
            added = true;
            break;
        }
    }
    if (!added) {
        new thread(c);
    }
}
