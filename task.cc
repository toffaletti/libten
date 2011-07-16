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
                if (c->exiting) {
                    delete c;
                } else {
                    runq.push_back(c);
                    l.unlock();
                    task::swap(&scheduler, c);
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
                    task *c = (task *)i->data.ptr;
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

task::task(const func_t &f_, size_t stack_size)
    : co((coroutine::proc)task::start, this, stack_size),
    f(f_), exiting(false)
{
}

task *task::self() {
    return runner::self()->get_task();
}

void task::swap(task *from, task *to) {
    // TODO: wrong place for this code. put in scheduler
    runner::self()->set_task(to);
    from->co.swap(&to->co);
}

task *task::spawn(const func_t &f) {
    task *c = new task(f);
    runner::self()->add_to_runqueue(c);
    return c;
}

void task::switch_() {
    task::swap(this, &runner::self()->scheduler);
}

void task::yield() {
   task::self()->switch_();
}

void task::start(task *c) {
    try {
        c->f();
    } catch(...) {
        abort();
    }
    c->exiting = true;
    c->switch_();
}

void task::migrate() {
    task *c = task::self();
    runner *t = runner::self();
    t->delete_from_runqueue(c);
    runner::add_to_empty_runqueue(c);
    task::yield();
    // will resume in other runner
}

void task::migrate_to(runner *to) {
    task *c = task::self();
    runner *from = runner::self();
    from->delete_from_runqueue(c);
    to->add_to_runqueue(c);
    task::yield();
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

void runner::poll(int fd, int events) {
    task *c = task::self();
    runner *t = runner::self();
    t->delete_from_runqueue(c);

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.ptr = c;
    assert(t->efd.add(fd, ev) == 0);
    // will be woken back up by epoll loop in schedule()
    task::yield();
}

