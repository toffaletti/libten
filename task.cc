#include "task.hh"

#include <iostream>

namespace detail {
__thread runner *runner_ = NULL;
}

runner::list runner::runners;
mutex runner::tmutex;

std::ostream &operator << (std::ostream &o, task_deque &l) {
    o << "[";
    for (task_deque::iterator i=l.begin(); i!=l.end(); ++i) {
        o << *i << ",";
    }
    o << "]";
    return o;
}

size_t runner::count() {
    mutex::scoped_lock l(runner::tmutex);
    return runner::runners.size();
}

typedef std::vector<epoll_event> event_vector;

void runner::schedule(bool loop) {
    for (;;) {
        {
            mutex::scoped_lock l(mut);
            while (!runq.empty()) {
                //std::cout << "runq: " << runq << "\n";
                task *c = runq.front();
                runq.pop_front();
                l.unlock();
                task::swap(&scheduler, c);
                switch (c->state) {
                case task::state_running:
                    c->state = task::state_idle;
                    l.lock();
                    runq.push_back(c);
                    break;
                case task::state_exiting:
                    delete c;
                    l.lock();
                    break;
                case task::state_migrating:
                    if (c->in) {
                        c->in->add_to_runqueue(c);
                    } else {
                        add_to_empty_runqueue(c);
                    }
                    l.lock();
                    break;
                default:
                    abort();
                    break;
                }
            }
        }

        event_vector events;
        while (efd.maxevents) {
            try {
                timespec now;
                // TODO: porbably should cache now for runner
                // avoid calling it every add_waiter()
                THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
                std::make_heap(waiters.begin(), waiters.end(), task_timeout_heap_compare());
                int timeout_ms = -1;
                if (!waiters.empty()) {
                    if (waiters.front()->ts <= now) {
                        // epoll_wait must return immediately
                        timeout_ms = 0;
                    } else {
                        timespec r = waiters.front()->ts - now;
                        timeout_ms = r.tv_sec * 1000;
                        timeout_ms += r.tv_nsec / 1000000;
                    }
                }

                efd.wait(events, timeout_ms);
                for (event_vector::const_iterator i=events.begin();
                    i!=events.end();++i)
                {
                    task *c = (task *)i->data.ptr;
                    add_to_runqueue(c);
                }

                // assume all EPOLLONESHOT
                //std::cout << "maxevents: " << efd.maxevents << "\n";
                efd.maxevents -= events.size();
                //std::cout << "maxevents: " << efd.maxevents << "\n";


                THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
                // fire expired timeouts
                // TODO: make sure task wasn't already triggered by epoll loop
                while (!waiters.empty() && waiters.front()->ts <= now) {
                    std::pop_heap(waiters.begin(), waiters.end(), task_timeout_heap_compare());
                    add_to_runqueue(waiters.back());
                    waiters.pop_back();
                }

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

void *runner::start(void *arg) {
    using namespace detail;
    runner_ = (runner *)arg;
    detail::runner_->append_to_list();
    try {
        detail::runner_->schedule();
    } catch (...) {}
    detail::runner_->remove_from_list();
    // TODO: if detatched, free memory here
    //if (detail::runner_->detached) {
    //    delete detail::runner_;
    //}
    return NULL;
}

task::task(const proc &f_, size_t stack_size)
    : co((coroutine::proc)task::start, this, stack_size),
    f(f_), state(state_idle)
{
}

task *task::self() {
    return runner::self()->get_task();
}

void task::swap(task *from, task *to) {
    // TODO: wrong place for this code. put in scheduler
    runner::self()->set_task(to);
    to->state = state_running;
    from->state = state_idle;
    from->co.swap(&to->co);
    from->state = state_running;
    // don't modify to state after
    // because it might have been changed
}

void task::spawn(const proc &f) {
    task *t = new task(f);
    runner::self()->add_to_runqueue(t);
}

void task::yield() {
    task::self()->co.swap(&runner::self()->scheduler.co);
}

void task::start(task *c) {
    try {
        c->f();
    } catch(...) {
        abort();
    }
    c->state = state_exiting;
    c->co.swap(&runner::self()->scheduler.co);
}

void task::migrate(runner *to) {
    task *c = task::self();
    // if to is NULL, first available runner is used
    // or a new one is spawned
    c->in = to;
    c->state = state_migrating;
    task::yield();
    // will resume in other runner
}

void runner::add_to_empty_runqueue(task *c) {
    mutex::scoped_lock l(tmutex);
    bool added = false;
    for (runner::list::iterator i=runners.begin(); i!=runners.end(); ++i) {
        //std::cout << "testing runner: " << *i << "\n";
        if ((*i)->add_to_runqueue_if_asleep(c)) {
            //std::cout << "added to runner: " << (*i) << "\n";
            added = true;
            break;
        }
    }
    if (!added) {
        new runner(c);
    }
}

void runner::add_waiter(task *t) {
    // XXX: not really thread safe
    // but we shouldn't be in runqueue
    assert(t != runq.back());

    timespec abs;
    THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &abs));
    t->ts += abs;
    waiters.push_back(t);
}

void task::poll(int fd, int events) {
    task *t = task::self();
    runner *r = runner::self();
    r->delete_from_runqueue(t);

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.ptr = t;
    assert(r->efd.add(fd, ev) == 0);
    // will be woken back up by epoll loop in schedule()
    task::yield();
}

