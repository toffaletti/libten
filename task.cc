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

void runner::run_queued_tasks() {
    mutex::scoped_lock l(mut);
    while (!runq.empty()) {
        task *c = runq.front();
        runq.pop_front();
        l.unlock();
        task::swap(&scheduler, c);
        switch (c->state) {
        case task::state_idle:
            // task wants to sleep
            l.lock();
            break;
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

void runner::check_io() {
    event_vector events(efd.maxevents ? efd.maxevents : 1);
    bool goto_runq = false;
    while (efd.maxevents || !waiters.empty()) {
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
                    // convert timespec to milliseconds
                    timeout_ms = r.tv_sec * 1000;
                    // 1 millisecond is 1 million nanoseconds
                    timeout_ms += r.tv_nsec / 1000000;
                }
            }

            if (events.empty()) {
                events.resize(efd.maxevents ? efd.maxevents : 1);
            }
            efd.wait(events, timeout_ms);
            for (event_vector::const_iterator i=events.begin();
                i!=events.end();++i)
            {
                task *c = (task *)i->data.ptr;
                goto_runq = true;
                add_to_runqueue(c);
            }

            // assume all EPOLLONESHOT
            //efd.maxevents -= events.size();

            THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
            // fire expired timeouts
            // TODO: make sure task wasn't already triggered by epoll loop
            while (!waiters.empty() && waiters.front()->ts <= now) {
                std::pop_heap(waiters.begin(), waiters.end(), task_timeout_heap_compare());
                goto_runq = true;
                task *t = waiters.back();
                if (add_to_runqueue(t)) {
                    // only timeout if epoll() loop didnt put it in runq
                    t->state = task::state_timeout;
                }
                waiters.pop_back();
            }

            // added some to runqueue, time to exit
            if (goto_runq) return;

        } catch (const std::exception &e) {
            abort();
        }
    }
}

void runner::schedule(bool loop) {
    for (;;) {
        run_queued_tasks();
        check_io();

        {
            // check_io might have filled runqueue again
            mutex::scoped_lock l(mut);
            if (!runq.empty()) continue;
        }

        if (loop)
            // block waiting for tasks to be scheduled on this runner
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
    if (to->state == state_idle)
        to->state = state_running;
    if (from->state == state_running)
        from->state = state_idle;
    from->co.swap(&to->co);
    if (from->state == state_idle)
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
        if ((*i)->add_to_runqueue_if_asleep(c)) {
            added = true;
            break;
        }
    }
    if (!added) {
        new runner(c);
    }
}

void task::sleep(unsigned int ms) {
    task *t = task::self();
    runner *r = runner::self();
    // convert milliseconds to seconds and nanoseconds
    t->ts.tv_sec = ms / 1000;
    t->ts.tv_nsec = (ms % 1000) * 1000000;
    r->add_waiter(t);
    task::yield();
}

void runner::add_waiter(task *t) {
    timespec abs;
    THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &abs));
    t->ts += abs;
    t->state = task::state_idle;
    waiters.push_back(t);
}

bool task::poll(int fd, int events, unsigned int ms) {
    task *t = task::self();
    runner *r = runner::self();
    // XXX: shouldn't be in the runqueue anyway
    //r->delete_from_runqueue(t);
    // TODO: maybe make a state for waiting io?
    t->state = state_idle;
    if (ms) {
        t->ts.tv_sec = ms / 1000;
        t->ts.tv_nsec = (ms % 1000) * 1000000;
        r->add_waiter(t);
    }

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.ptr = t;
    assert(r->efd.add(fd, ev) == 0);
    // will be woken back up by epoll loop in schedule()
    task::yield();
    assert(r->efd.remove(fd) == 0);
    return t->state != state_timeout;
}

