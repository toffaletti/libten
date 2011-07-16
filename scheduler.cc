#include "scheduler.hh"

#include <iostream>

namespace scheduler {

namespace detail {
__thread thread *thread_ = NULL;
}

thread::list thread::threads;
mutex thread::tmutex;

std::ostream &operator << (std::ostream &o, coro_deque &l) {
    o << "[";
    for (coro_deque::iterator i=l.begin(); i!=l.end(); ++i) {
        o << *i << ",";
    }
    o << "]";
    return o;
}

size_t thread::count() {
    mutex::scoped_lock l(thread::tmutex);
    return thread::threads.size();
}

typedef std::vector<epoll_event> event_vector;

void thread::schedule(bool loop) {
    for (;;) {
        {
            mutex::scoped_lock l(mut);
            while (!runq.empty()) {
                //std::cout << "runq: " << runq << "\n";
                coroutine *c = runq.front();
                runq.pop_front();
                if (c->exiting) {
                    delete c;
                } else {
                    runq.push_back(c);
                    l.unlock();
                    coroutine::swap(&scheduler, c);
                    l.lock();
                }
            }
        }

        event_vector events;
        while (efd.maxevents) {
            try {
                efd.wait(events);
                for (event_vector::const_iterator i=events.begin();
                    i!=events.end();++i)
                {
                    coroutine *c = (coroutine *)i->data.ptr;
                    add_to_runqueue(c);
                }
                // assume all EPOLLONESHOT
                //std::cout << "maxevents: " << efd.maxevents << "\n";
                efd.maxevents -= events.size();
                //std::cout << "maxevents: " << efd.maxevents << "\n";
            } catch (const std::exception &e) {
                //std::cout << "epoll wait error: " << e.what() << "\n";
            }
        }

        {
            // epoll loop might have filled runqueue again
            mutex::scoped_lock l(mut);
            if (!runq.empty()) continue;
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
    //if (detail::thread_->detached) {
    //    delete detail::thread_;
    //}
    return NULL;
}

coroutine::coroutine(const func_t &f_, size_t stack_size_)
    : f(f_), exiting(false), stack_size(stack_size_)
{
    // add on size for a guard page
    size_t real_size = stack_size + getpagesize();
    int r = posix_memalign((void **)&stack, getpagesize(), real_size);
    THROW_ON_NONZERO(r);
    // protect the guard page
    THROW_ON_ERROR(mprotect(stack+stack_size, getpagesize(), PROT_NONE));
    getcontext(&context);
#ifndef NVALGRIND
    valgrind_stack_id =
        VALGRIND_STACK_REGISTER(stack, stack+stack_size);
#endif
    context.uc_stack.ss_sp = stack;
    context.uc_stack.ss_size = stack_size;
    context.uc_link = 0;
    // this depends on glibc's work around that allows
    // pointers to be passed to makecontext
    // it also depends on g++ allowing static C++
    // functions to be used for C linkage
    makecontext(&context, (void (*)())coroutine::start, 1, this);
}

coroutine *coroutine::self() {
    return thread::self()->get_coro();
}

void coroutine::swap(coroutine *from, coroutine *to) {
    // TODO: wrong place for this code. put in scheduler
    thread::self()->set_coro(to);
    //std::cout << "swapping from " <<
    //    &(from->context) << " to " << &(to->context) <<
    //    " in thread: " << thread::self()->id() << "\n";
    swapcontext(&from->context, &to->context);
}

coroutine *coroutine::spawn(const func_t &f) {
    coroutine *c = new coroutine(f);
    thread::self()->add_to_runqueue(c);
    return c;
}

void coroutine::switch_() {
    coroutine::swap(this, &thread::self()->scheduler);
}

void coroutine::yield() {
    //std::cout << "coro " << coroutine::self() << " yielding in thread: " << thread::self()->id() << "\n";
    coroutine::self()->switch_();
}

void coroutine::start(coroutine *c) {
    try {
        c->f();
    } catch(...) {
        abort();
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
        //std::cout << "testing thread: " << *i << "\n";
        if ((*i)->add_to_runqueue_if_asleep(c)) {
            //std::cout << "added to thread: " << (*i) << "\n";
            added = true;
            break;
        }
    }
    if (!added) {
        new thread(c);
    }
}

void thread::poll(int fd, int events) {
    coroutine *c = coroutine::self();
    thread *t = thread::self();
    t->delete_from_runqueue(c);

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.ptr = c;
    assert(t->efd.add(fd, ev) == 0);
    // will be woken back up by epoll loop in schedule()
    coroutine::yield();
}

} // end namespace scheduler

