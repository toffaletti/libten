#include "runner.hh"
#include <cxxabi.h>

#include <stdexcept>
#include <algorithm>
#include <set>

namespace detail {
__thread runner *runner_ = NULL;
runner::list *runners = new runner::list;
mutex *tmutex = new mutex;
}

static unsigned int timespec_to_milliseconds(const timespec &ts) {
    // convert timespec to milliseconds
    unsigned int ms = ts.tv_sec * 1000;
    // 1 millisecond is 1 million nanoseconds
    ms += ts.tv_nsec / 1000000;
    return ms;
}

static std::ostream &operator << (std::ostream &o, task::deque &l) {
    o << "[";
    for (task::deque::iterator i=l.begin(); i!=l.end(); ++i) {
        o << *i << ",";
    }
    o << "]";
    return o;
}

static void append_to_list(runner *r) {
    mutex::scoped_lock l(*detail::tmutex);
    detail::runners->push_back(r);
}

static void remove_from_list(runner *r) {
    mutex::scoped_lock l(*detail::tmutex);
    detail::runners->remove(r);
}

static void wakeup_all_runners() {
    mutex::scoped_lock l(*detail::tmutex);
    runner *current = runner::self();
    for (runner::list::iterator i=detail::runners->begin(); i!=detail::runners->end(); ++i) {
        if (current == *i) continue;
        (*i)->wakeup();
    }
}

struct task_timeout_heap_compare {
    bool operator ()(const task *a, const task *b) const {
        // handle -1 case, which we want at the end
        if (a->get_timeout().tv_sec < 0) return true;
        return a->get_timeout() > b->get_timeout();
    }
};

template <typename SetT> struct in_set {
    SetT &set;

    in_set(SetT &s) : set(s) {}

    bool operator()(typename SetT::key_type &k) {
        return set.count(k) > 0;
    }
};

runner *runner::self() {
    if (detail::runner_ == NULL) {
        // this should happen in the main thread only and only once
        detail::runner_ = new runner;
    }
    return detail::runner_;
}

void runner::sleep(mutex::scoped_lock &l) {
    // move to the head of the list
    // to simulate a FIFO
    // if there are too many threads
    // and not enough tasks, we can
    // use a cond.timedwait here and
    // exit a thread that times out

    // must set asleep here
    // another thread could add to runq
    // (a resume() for example)
    // during the unlock and if we're
    // not marked asleep, then we
    // end up sleeping while runq isn't empty
    asleep = true;
    l.unlock();
    {
        mutex::scoped_lock tl(*detail::tmutex);
        detail::runners->remove(this);
        detail::runners->push_front(this);
    }
    l.lock();
    while (asleep) {
        cond.wait(l);
    }
}


typedef std::vector<epoll_event> event_vector;

void runner::run_queued_tasks() {
    mutex::scoped_lock l(mut);
    while (!runq.empty()) {
        task *t = runq.front();
        runq.pop_front();
        l.unlock();
        task::swap(&scheduler, t);
        switch (t->get_state()) {
        case task::state_idle:
            // task wants to sleep
            l.lock();
            break;
        case task::state_running:
            t->set_state(task::state_idle, "idle");
            l.lock();
            runq.push_back(t);
            break;
        case task::state_exiting:
            delete t;
            l.lock();
            break;
        case task::state_migrating:
            if (t->get_runner()) {
                t->get_runner()->add_to_runqueue(t);
            } else {
                add_to_empty_runqueue(t);
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
        if (waiters.front()->get_timeout().tv_sec > 0) {
            if (waiters.front()->get_timeout() <= now) {
                // epoll_wait must return immediately
                timeout_ms = 0;
            } else {
                timeout_ms = timespec_to_milliseconds(waiters.front()->get_timeout() - now);
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
            task *t = pollfds[i->data.fd].t;
            pollfd *pfd = pollfds[i->data.fd].pfd;
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
            if (t->get_timeout().tv_sec > 0 && t->get_timeout() <= now) {
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
    append_to_list(detail::runner_);
    try {
        for (;;) {
            run_queued_tasks();
            check_io();

            // check_io might have filled runqueue again
            mutex::scoped_lock l(mut);
            if (runq.empty()) {
                if (task::get_ntasks() > 0) {
                    // block waiting for tasks to be scheduled on this runner
                    sleep(l);
                } else {
                    l.unlock();
                    wakeup_all_runners();
                    break;
                }
            }
        }
    } catch (abi::__forced_unwind&) {
        remove_from_list(detail::runner_);
        throw;
    } catch (std::exception &e) {
        remove_from_list(detail::runner_);
        throw;
    }
    remove_from_list(detail::runner_);
}

void *runner::start(void *arg) {
    using namespace detail;
    runner_ = (runner *)arg;
    thread::self().detach();
    try {
        detail::runner_->schedule();
    } catch (abi::__forced_unwind&) {
        throw;
    } catch (std::exception &e) {
        fprintf(stderr, "uncaught exception in runner: %s\n", e.what());
    }
    delete detail::runner_;
    detail::runner_ = NULL;
    return NULL;
}

void runner::add_to_empty_runqueue(task *c) {
    mutex::scoped_lock l(*detail::tmutex);
    bool added = false;
    for (runner::list::iterator i=detail::runners->begin(); i!=detail::runners->end(); ++i) {
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
    if (t->get_timeout().tv_sec != -1) {
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &abs));
        t->set_abs_timeout(t->get_timeout() + abs);
    }
    t->set_state(task::state_idle, "waiting");
    waiters.push_back(t);
}

