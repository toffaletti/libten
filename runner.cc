#include "runner.hh"

#include <algorithm>
#include <set>

namespace detail {
__thread runner *runner_ = NULL;
}

// static
runner::list runner::runners;
mutex runner::tmutex;

static unsigned int timespec_to_milliseconds(const timespec &ts) {
    // convert timespec to milliseconds
    unsigned int ms = ts.tv_sec * 1000;
    // 1 millisecond is 1 million nanoseconds
    ms += ts.tv_nsec / 1000000;
    return ms;
}

static std::ostream &operator << (std::ostream &o, task_deque &l) {
    o << "[";
    for (task_deque::iterator i=l.begin(); i!=l.end(); ++i) {
        o << *i << ",";
    }
    o << "]";
    return o;
}

struct task_timeout_heap_compare {
    bool operator ()(const task *a, const task *b) const {
        // handle -1 case, which we want at the end
        if (a->ts.tv_sec < 0) return true;
        return a->ts > b->ts;
    }
};

template <typename SetT> struct in_set {
    SetT &set;

    in_set(SetT &s) : set(s) {}

    bool operator()(typename SetT::key_type &k) {
        return set.count(k) > 0;
    }
};


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
    if (waiters.empty()) return;
    bool done = false;
    while (!done) {
        timespec now;
        // TODO: probably should cache now for runner
        // avoid calling it every add_waiter()
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        std::make_heap(waiters.begin(), waiters.end(), task_timeout_heap_compare());
        int timeout_ms = -1;
        assert(!waiters.empty());
        if (waiters.front()->ts.tv_sec > 0) {
            if (waiters.front()->ts <= now) {
                // epoll_wait must return immediately
                timeout_ms = 0;
            } else {
                timeout_ms = timespec_to_milliseconds(waiters.front()->ts - now);
                // help avoid spinning on timeouts smaller than 1 ms
                if (timeout_ms <= 0) timeout_ms = 1;
            }
        }

        if (events.empty()) {
            events.resize(efd.maxevents ? efd.maxevents : 1);
        }
        efd.wait(events, timeout_ms);

        std::set<task *> wake_tasks;
        for (event_vector::const_iterator i=events.begin();
            i!=events.end();++i)
        {
            task *t = pollfds[i->data.fd].first;
            pollfd *pfd = pollfds[i->data.fd].second;
            if (t && pfd) {
                pfd->revents = i->events;
                add_to_runqueue(t);
                wake_tasks.insert(t);
            } else {
                // TODO: otherwise we might want to remove fd from epoll
                fprintf(stderr, "event for fd: %i but has no task\n", i->data.fd);
            }
        }

        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        for (task_heap::iterator i=waiters.begin(); i!=waiters.end(); ++i) {
            task *t = *i;
            if (t->ts.tv_sec > 0 && t->ts <= now) {
                wake_tasks.insert(t);
                add_to_runqueue(t);
            }
        }

        task_heap::iterator nend =
            std::remove_if(waiters.begin(), waiters.end(), in_set<std::set<task *> >(wake_tasks));
        waiters.erase(nend, waiters.end());
        // there are tasks to wake up!
        if (!wake_tasks.empty()) done = true;
    }
}

void runner::schedule() {
    detail::runner_->append_to_list();
    try {
        for (;;) {
            run_queued_tasks();
            check_io();

            // check_io might have filled runqueue again
            mutex::scoped_lock l(mut);
            if (runq.empty()) {
                if (task::ntasks > 0) {
                    // block waiting for tasks to be scheduled on this runner
                    sleep(l);
                } else {
                    l.unlock();
                    wakeup_all_runners();
                    break;
                }
            }
        }
    } catch (...) {}
    detail::runner_->remove_from_list();
}

void *runner::start(void *arg) {
    using namespace detail;
    runner_ = (runner *)arg;
    thread::self().detach();
    try {
        detail::runner_->schedule();
    } catch (...) {}
    delete detail::runner_;
    detail::runner_ = NULL;
    return NULL;
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

void runner::add_waiter(task *t) {
    timespec abs;
    if (t->ts.tv_sec != -1) {
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &abs));
        t->ts += abs;
    }
    t->state = task::state_idle;
    waiters.push_back(t);
}

