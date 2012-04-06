#ifndef TASK_IO_HH
#define TASK_IO_HH

#include "task/proc.hh"

namespace ten {

struct io_scheduler {
    struct task_timeout_compare {
        bool operator ()(const task *a, const task *b) const {
            return (a->timeouts.front()->when < b->timeouts.front()->when);
        }
    };
    struct task_poll_state {
        task *t_in; // POLLIN task
        pollfd *p_in; // pointer to pollfd structure that is on the task's stack
        task *t_out; // POLLOUT task
        pollfd *p_out; // pointer to pollfd structure that is on the task's stack
        uint32_t events; // events this fd is registered for
        task_poll_state() : t_in(0), p_in(0), t_out(0), p_out(0), events(0) {}
    };
    typedef std::vector<task_poll_state> poll_task_array;
    typedef std::vector<epoll_event> event_vector;

    //! tasks with pending timeouts
    std::vector<task *> timeout_tasks;
    //! array of tasks waiting on fds, indexed by the fd for speedy lookup
    poll_task_array pollfds;
    //! epoll events
    event_vector events;
    //! the epoll fd used for io in this runner
    epoll_fd efd;
    //! number of fds we've been asked to wait on
    size_t npollfds;

    io_scheduler() : npollfds(0) {
        events.reserve(1000);
        // add the eventfd used to wake up
        epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLET;
        int e_fd = _this_proc->event.fd;
        ev.data.fd = e_fd;
        if (pollfds.size() <= (size_t)e_fd) {
            pollfds.resize(e_fd+1);
        }
        efd.add(e_fd, ev);
        taskspawn(std::bind(&io_scheduler::fdtask, this));
    }

    void add_pollfds(task *t, pollfd *fds, nfds_t nfds) {
        for (nfds_t i=0; i<nfds; ++i) {
            epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            int fd = fds[i].fd;
            fds[i].revents = 0;
            // make room for the highest fd number
            if (pollfds.size() <= (size_t)fd) {
                pollfds.resize(fd+1);
            }
            ev.data.fd = fd;
            uint32_t events = pollfds[fd].events;

            if (fds[i].events & EPOLLIN) {
                CHECK(pollfds[fd].t_in == 0) << "fd: " << fd << " from " << t << " but " << pollfds[fd].t_in;
                pollfds[fd].t_in = t;
                pollfds[fd].p_in = &fds[i];
                pollfds[fd].events |= EPOLLIN;
            }

            if (fds[i].events & EPOLLOUT) {
                CHECK(pollfds[fd].t_out == 0) << "fd: " << fd << " from " << t << " but " << pollfds[fd].t_out;
                pollfds[fd].t_out = t;
                pollfds[fd].p_out = &fds[i];
                pollfds[fd].events |= EPOLLOUT;
            }

            ev.events = pollfds[fd].events;

            if (events == 0) {
                THROW_ON_ERROR(efd.add(fd, ev));
            } else if (events != pollfds[fd].events) {
                THROW_ON_ERROR(efd.modify(fd, ev));
            }
            ++npollfds;
        }
    }

    int remove_pollfds(pollfd *fds, nfds_t nfds) {
        int rvalue = 0;
        for (nfds_t i=0; i<nfds; ++i) {
            int fd = fds[i].fd;
            if (fds[i].revents) rvalue++;

            if (pollfds[fd].p_in == &fds[i]) {
                pollfds[fd].t_in = 0;
                pollfds[fd].p_in = 0;
                pollfds[fd].events ^= EPOLLIN;
            }

            if (pollfds[fd].p_out == &fds[i]) {
                pollfds[fd].t_out = 0;
                pollfds[fd].p_out = 0;
                pollfds[fd].events ^= EPOLLOUT;
            }

            if (pollfds[fd].events == 0) {
                efd.remove(fd);
            } else {
                epoll_event ev;
                memset(&ev, 0, sizeof(ev));
                ev.data.fd = fd;
                ev.events = pollfds[fd].events;
                THROW_ON_ERROR(efd.modify(fd, ev));
            }
            --npollfds;
        }
        return rvalue;
    }

    template<typename Rep, typename Period, typename ExceptionT>
    task::timeout_t *add_timeout(task *t, const duration<Rep,Period> &dura, ExceptionT e) {
        task::timeout_t *to = t->add_timeout(dura, e);
        auto i = std::lower_bound(timeout_tasks.begin(), timeout_tasks.end(), t, task_timeout_compare());
        timeout_tasks.insert(i, t);
        return to;
    }

    template<typename Rep,typename Period>
    task::timeout_t *add_timeout(task *t, const duration<Rep,Period> &dura) {
        task::timeout_t *to = t->add_timeout(dura);
        auto i = std::lower_bound(timeout_tasks.begin(), timeout_tasks.end(), t, task_timeout_compare());
        timeout_tasks.insert(i, t);
        return to;
    }

    template<typename Rep,typename Period>
    void sleep(const duration<Rep, Period> &dura) {
        task *t = _this_proc->ctask;
        add_timeout(t, dura);
        t->swap();
    }

    void remove_timeout_task(task *t) {
        CHECK(t->timeouts.empty());
        auto i = std::remove(timeout_tasks.begin(), timeout_tasks.end(), t);
        timeout_tasks.erase(i, timeout_tasks.end());
    }

    bool fdwait(int fd, int rw, uint64_t ms) {
        short events = 0;
        switch (rw) {
            case 'r':
                events |= EPOLLIN;
                break;
            case 'w':
                events |= EPOLLOUT;
                break;
        }
        pollfd fds = {fd, events, 0};
        if (poll(&fds, 1, ms) > 0) {
            if ((fds.revents & EPOLLERR) || (fds.revents & EPOLLHUP)) {
                return false;
            }
            return true;
        }
        return false;
    }

    int poll(pollfd *fds, nfds_t nfds, uint64_t ms) {
        task *t = _this_proc->ctask;
        if (nfds == 1) {
            taskstate("poll fd %i r: %i w: %i %ul ms",
                    fds->fd, fds->events & EPOLLIN, fds->events & EPOLLOUT, ms);
        } else {
            taskstate("poll %u fds for %ul ms", nfds, ms);
        }
        task::timeout_t *timeout_id = 0;
        if (ms) {
            timeout_id = add_timeout(t, milliseconds(ms));
        }
        add_pollfds(t, fds, nfds);

        DVLOG(5) << "task: " << t << " poll for " << nfds << " fds";
        try {
            t->swap();
        } catch (...) {
            if (timeout_id) {
                t->remove_timeout(timeout_id);
            }
            remove_pollfds(fds, nfds);
            throw;
        }

        if (timeout_id) {
            t->remove_timeout(timeout_id);
        }
        return remove_pollfds(fds, nfds);
    }

    void fdtask() {
        taskname("fdtask");
        tasksystem();
        proc *p = _this_proc;
        for (;;) {
            p->now = monotonic_clock::now();
            // let everyone else run
            taskyield();
            p->now = monotonic_clock::now();
            task *t = 0;

            int ms = -1;
            // lock must be held while determining whether or not we'll be
            // asleep in epoll, so wakeupandunlock will work from another
            // thread
            std::unique_lock<std::mutex> lk(p->mutex);
            if (!timeout_tasks.empty()) {
                t = timeout_tasks.front();
                CHECK(!t->timeouts.empty()) << t << " in timeout list with no timeouts set";
                if (t->timeouts.front()->when <= p->now) {
                    // epoll_wait must return asap
                    ms = 0;
                } else {
                    ms = duration_cast<milliseconds>(t->timeouts.front()->when - p->now).count();
                    // avoid spinning on timeouts smaller than 1ms
                    if (ms <= 0) ms = 1;
                }
            }

            if (ms != 0) {
                if (!p->runqueue.empty()) {
                    // don't block on epoll if tasks are ready to run
                    ms = 0;
                }
            }

            if (ms != 0 || npollfds > 0) {
                taskstate("epoll %d ms", ms);
                // only process 1000 events each iteration to keep it fair
                if (ms > 1 || ms < 0) {
                    p->polling = true;
                }
                lk.unlock();
                events.resize(1000);
                efd.wait(events, ms);
                lk.lock();
                p->polling = false;
                int e_fd = p->event.fd;
                lk.unlock();
                // wake up io tasks
                for (auto i=events.cbegin(); i!=events.cend(); ++i) {
                    // NOTE: epoll will also return EPOLLERR and EPOLLHUP for every fd
                    // even if they arent asked for, so we must wake up the tasks on any event
                    // to avoid just spinning in epoll.
                    int fd = i->data.fd;
                    if (pollfds[fd].t_in) {
                        pollfds[fd].p_in->revents = i->events;
                        t = pollfds[fd].t_in;
                        DVLOG(5) << "IN EVENT on task: " << t;
                        t->ready();
                    }

                    // check to see if pollout is a different task than pollin
                    if (pollfds[fd].t_out && pollfds[fd].t_out != pollfds[fd].t_in) {
                        pollfds[fd].p_out->revents = i->events;
                        t = pollfds[fd].t_out;
                        DVLOG(5) << "OUT EVENT on task: " << t;
                        t->ready();
                    }

                    if (i->data.fd == e_fd) {
                        // our wake up eventfd was written to
                        // clear events by reading value
                        p->event.read();
                    } else if (pollfds[fd].t_in == 0 && pollfds[fd].t_out == 0) {
                        // TODO: otherwise we might want to remove fd from epoll
                        LOG(ERROR) << "event " << i->events << " for fd: "
                            << i->data.fd << " but has no task";
                    }
                }
            }

            // must unlock before calling task::ready
            if (lk.owns_lock()) lk.unlock();
            p->now = monotonic_clock::now();
            // wake up sleeping tasks
            auto i = timeout_tasks.begin();
            for (; i != timeout_tasks.end(); ++i) {
                t = *i;
                if (t->timeouts.front()->when <= p->now) {
                    DVLOG(5) << "TIMEOUT on task: " << t;
                    t->ready();
                } else {
                    break;
                }
            }
        }
        DVLOG(5) << "BUG: " << _this_proc->ctask << " is exiting";
    }
};

} // end namespace ten

#endif // TASK_IO_HH

