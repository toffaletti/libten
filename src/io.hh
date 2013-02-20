#ifndef LIBTEN_IO_HH
#define LIBTEN_IO_HH

#include "alarm.hh"
#include "thread_context.hh"

namespace ten {

struct io_scheduler {
    typedef ten::alarm_clock<ptr<task::pimpl>, proc_clock_t> alarm_clock;

    struct task_poll_state {
        ptr<task::pimpl> t_in = nullptr; // POLLIN task
        pollfd *p_in = nullptr; // pointer to pollfd structure that is on the task's stack
        ptr<task::pimpl> t_out = nullptr; // POLLOUT task
        pollfd *p_out = nullptr; // pointer to pollfd structure that is on the task's stack
        uint32_t events = 0; // events this fd is registered for
    };
    typedef std::vector<task_poll_state> poll_task_array;
    typedef std::vector<epoll_event> event_vector;

    //! tasks with pending timeouts
    alarm_clock alarms;
    //! array of tasks waiting on fds, indexed by the fd for speedy lookup
    poll_task_array pollfds;
    //! epoll events
    event_vector events;
    //! timerfd used for breaking epoll_wait for timeouts
    // required because the timeout value for epoll_wait is not accurate
    timer_fd tfd;
    //! the epoll fd used for io in this runner
    epoll_fd efd;
    //! number of fds we've been asked to wait on
    size_t npollfds;
    std::shared_ptr<proc_waker> _waker;

    io_scheduler() : npollfds(0) {
        _waker = this_ctx->scheduler.get_waker();
        events.reserve(1000);
        // add the eventfd used to wake up
        {
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLET;
            int e_fd = _waker->event.fd;
            ev.data.fd = e_fd;
            if (pollfds.size() <= (size_t)e_fd) {
                pollfds.resize(e_fd+1);
            }
            efd.add(e_fd, ev);
        }

        {
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLET;
            int e_fd = tfd.fd;
            ev.data.fd = e_fd;
            if (pollfds.size() <= (size_t)e_fd) {
                pollfds.resize(e_fd+1);
            }
            efd.add(e_fd, ev);
        }
        taskspawn(std::bind(&io_scheduler::fdtask, this));
    }

    void add_pollfds(ptr<task::pimpl> t, pollfd *fds, nfds_t nfds) {
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
            uint32_t saved_events = pollfds[fd].events;

            if (fds[i].events & EPOLLIN) {
                DCHECK(pollfds[fd].t_in == nullptr) << "BUG: fd: " << fd << " from " << t << " but " << pollfds[fd].t_in;
                pollfds[fd].t_in = t;
                pollfds[fd].p_in = &fds[i];
                pollfds[fd].events |= EPOLLIN;
            }

            if (fds[i].events & EPOLLOUT) {
                DCHECK(pollfds[fd].t_out == nullptr) << "BUG: fd: " << fd << " from " << t << " but " << pollfds[fd].t_out;
                pollfds[fd].t_out = t;
                pollfds[fd].p_out = &fds[i];
                pollfds[fd].events |= EPOLLOUT;
            }

            ev.events = pollfds[fd].events;

            if (saved_events == 0) {
                throw_if(efd.add(fd, ev) == -1);
            } else if (saved_events != pollfds[fd].events) {
                throw_if(efd.modify(fd, ev) == -1);
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
                pollfds[fd].t_in.reset();
                pollfds[fd].p_in = nullptr;
                pollfds[fd].events ^= EPOLLIN;
            }

            if (pollfds[fd].p_out == &fds[i]) {
                pollfds[fd].t_out.reset();
                pollfds[fd].p_out = nullptr;
                pollfds[fd].events ^= EPOLLOUT;
            }

            if (pollfds[fd].events == 0) {
                efd.remove(fd);
            } else {
                epoll_event ev;
                memset(&ev, 0, sizeof(ev));
                ev.data.fd = fd;
                ev.events = pollfds[fd].events;
                throw_if(efd.modify(fd, ev) == -1);
            }
            --npollfds;
        }
        return rvalue;
    }

    bool fdwait(int fd, int rw, optional_timeout ms) {
        short events_ = 0;
        switch (rw) {
            case 'r':
                events_ |= EPOLLIN;
                break;
            case 'w':
                events_ |= EPOLLOUT;
                break;
        }
        pollfd fds = {fd, events_, 0};
        if (poll(&fds, 1, ms) > 0) {
            if ((fds.revents & EPOLLERR) || (fds.revents & EPOLLHUP)) {
                return false;
            }
            return true;
        }
        return false;
    }

    int poll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
        ptr<task::pimpl> t = this_ctx->scheduler.current_task();
        if (nfds == 1) {
            taskstate("poll fd %i r: %i w: %i %ul ms",
                    fds->fd,
                    fds->events & EPOLLIN,
                    fds->events & EPOLLOUT,
                    ms ? ms->count() : 0);
        } else {
            taskstate("poll %u fds for %ul ms", nfds, ms ? ms->count() : 0);
        }
        add_pollfds(t, fds, nfds);

        DVLOG(5) << "task: " << t << " poll for " << nfds << " fds";
        try {
            optional<alarm_clock::scoped_alarm> timeout_alarm;
            if (ms) {
                auto now = this_ctx->scheduler.cached_time();
                timeout_alarm.emplace(alarms, t, now + *ms);
            }
            t->swap();
        } catch (...) {
            remove_pollfds(fds, nfds);
            throw;
        }

        return remove_pollfds(fds, nfds);
    }

    void fdtask() {
        taskname("fdtask");
        tasksystem();
        for (;;) {
            this_ctx->scheduler.update_cached_time();
            // let everyone else run
            taskyield();
            this_ctx->scheduler.update_cached_time();
            ptr<task::pimpl> t = nullptr;

            int ms = -1;
            // lock must be held while determining whether or not we'll be
            // asleep in epoll, so wakeupandunlock will work from another
            // thread
            optional<proc_time_t> timeout_when = alarms.when();
            if (timeout_when) {
                auto now = this_ctx->scheduler.cached_time();
                if (*timeout_when <= now) {
                    // epoll_wait must return asap
                    ms = 0;
                } else {
                    uint64_t usec = duration_cast<microseconds>(*timeout_when - now).count();
                    // TODO: round millisecond timeout up for now
                    // in the future we can provide higher resolution thanks to
                    // timerfd supporting nanosecond resolution
                    ms = (usec + (1000/2)) / 1000;
                    // avoid spinning on timeouts smaller than 1ms
                    if (ms <= 0) ms = 1;
                }
            }

            if (ms != 0) {
                if (this_ctx->scheduler.is_ready()) {
                    // don't block on epoll if tasks are ready to run
                    ms = 0;
                }
            }

            if (ms != 0 || npollfds > 0) {
                taskstate("epoll %d ms", ms);
                if (ms > 0) {
                    struct itimerspec tspec{};
                    struct itimerspec oldspec{};
                    tspec.it_value.tv_sec = ms / 1000;
                    tspec.it_value.tv_nsec = (ms % 1000) * 1000000;
                    tfd.settime(0, tspec, oldspec);
                    // epoll_wait timeout is not accurate so we use timerfd
                    // to break from epoll_wait instead of the timeout value
                    // -1 means no timeout.
                    ms = -1;
                }
                // only process 100 events each iteration to keep it fair
                events.resize(100);
                _waker->polling = true;
                if (this_ctx->scheduler.is_dirty()) {
                    // another thread(s) changed the dirtyq before we started
                    // polling. so we should skip epoll and run that task
                    events.resize(0);
                } else {
                    efd.wait(events, ms);
                }
                _waker->polling = false;
                int e_fd = _waker->event.fd;
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
                        t->ready_for_io();
                    }

                    // check to see if pollout is a different task than pollin
                    if (pollfds[fd].t_out && pollfds[fd].t_out != pollfds[fd].t_in) {
                        pollfds[fd].p_out->revents = i->events;
                        t = pollfds[fd].t_out;
                        DVLOG(5) << "OUT EVENT on task: " << t;
                        t->ready_for_io();
                    }

                    if (i->data.fd == e_fd) {
                        // our wake up eventfd was written to
                        // clear events by reading value
                        _waker->event.read();
                    } else if (i->data.fd == tfd.fd) {
                        // timerfd fired, sleeping tasks woken below
                    } else if (pollfds[fd].t_in == nullptr && pollfds[fd].t_out == nullptr) {
                        // TODO: otherwise we might want to remove fd from epoll
                        LOG(ERROR) << "event " << i->events << " for fd: "
                            << i->data.fd << " but has no task";
                    }
                }
            }

            auto now = this_ctx->scheduler.update_cached_time();
            // wake up sleeping tasks
            alarms.tick(now, [this](ptr<task::pimpl> t, std::exception_ptr exception) {
                if (exception != nullptr && t->exception == nullptr) {
                    DVLOG(5) << "alarm with exception fired: " << t;
                    t->exception = exception;
                }
                DVLOG(5) << "TIMEOUT on task: " << t;
                t->ready_for_io();
            });
        }
        DVLOG(5) << "BUG: " << this_ctx->scheduler.current_task() << " is exiting";
    }
};

} // end namespace ten

#endif // LIBTEN_IO_HH
