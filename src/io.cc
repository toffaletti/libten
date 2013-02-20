#include "io.hh"
#include "thread_context.hh"

namespace ten {

io::io() : _evfd(0, EFD_NONBLOCK), npollfds(0) {
    events.reserve(100);
    // add the eventfd used to wake up
    {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        int e_fd = _evfd.fd;
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
}

void io::add_pollfds(ptr<task::pimpl> t, pollfd *fds, nfds_t nfds) {
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

int io::remove_pollfds(pollfd *fds, nfds_t nfds) {
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

bool io::fdwait(int fd, int rw, optional_timeout ms) {
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

int io::poll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
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
        optional<scheduler::alarm_clock::scoped_alarm> timeout_alarm;
        if (ms) {
            auto now = this_ctx->scheduler.cached_time();
            timeout_alarm.emplace(this_ctx->scheduler.alarms, t, now + *ms);
        }
        t->swap();
    } catch (...) {
        remove_pollfds(fds, nfds);
        throw;
    }

    return remove_pollfds(fds, nfds);
}

void io::wakeup() {
    _evfd.write(1);
}

void io::wait(optional<proc_time_t> when) {
    this_ctx->scheduler.update_cached_time();
    ptr<task::pimpl> t = nullptr;

    int ms = -1;
    if (when) {
        auto now = this_ctx->scheduler.cached_time();
        if (*when <= now) {
            // epoll_wait must return asap
            ms = 0;
        } else {
            uint64_t usec = duration_cast<microseconds>(*when - now).count();
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
        //taskstate("epoll %d ms", ms);
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
        if (this_ctx->scheduler.is_dirty()) {
            // another thread(s) changed the dirtyq before we started
            // polling. so we should skip epoll and run that task
            events.resize(0);
        } else {
            efd.wait(events, ms);
        }
        for (auto i=events.cbegin(); i!=events.cend(); ++i) {
            // NOTE: epoll will also return EPOLLERR and EPOLLHUP for every fd
            // even if they arent asked for, so we must wake up the tasks on any event
            // to avoid just spinning in epoll.
            int fd = i->data.fd;
            if (fd == _evfd.fd) {
                // our wake up eventfd was written to
                // clear events by reading value
                _evfd.read();
            } else if (fd == tfd.fd) {
                // timerfd fired for sleeping/timeout tasks
            } else if ((size_t)fd < pollfds.size()) {
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

                if (pollfds[fd].t_in == nullptr && pollfds[fd].t_out == nullptr) {
                    // TODO: otherwise we might want to remove fd from epoll
                    LOG(ERROR) << "event " << i->events << " for fd: "
                        << i->data.fd << " but has no task";
                }
            } else {
                LOG(ERROR) << "BUG: mystery fd " << fd;
            }
        }
    }
}



} // ten
