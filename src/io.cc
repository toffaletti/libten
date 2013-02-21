#include "io.hh"
#include "thread_context.hh"

namespace ten {

io::io() {
    _events.reserve(100);
    // add the eventfd used to wake up
    {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        int e_fd = _evfd.fd;
        ev.data.fd = e_fd;
        throw_if(_efd.add(e_fd, ev) == -1);
    }

    // add timerfd used for timeouts
    {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        int e_fd = _tfd.fd;
        ev.data.fd = e_fd;
        throw_if(_efd.add(e_fd, ev) == -1);
    }
}

void io::add_pollfds(ptr<task::pimpl> t, pollfd *fds, nfds_t nfds) {
    for (nfds_t i=0; i<nfds; ++i) {
        epoll_event ev{};
        int fd = fds[i].fd;
        fds[i].revents = 0;
        // make room for the highest fd number
        if (_pollfds.size() <= (size_t)fd) {
            _pollfds.resize(fd+1);
        }
        ev.data.fd = fd;
        uint32_t saved_events = _pollfds[fd].events;

        if (fds[i].events & EPOLLIN) {
            DCHECK(_pollfds[fd].t_in == nullptr)
                << "BUG: fd: " << fd << " from " << t << " but " << _pollfds[fd].t_in;
            _pollfds[fd].t_in = t;
            _pollfds[fd].p_in = &fds[i];
            _pollfds[fd].events |= EPOLLIN;
        }

        if (fds[i].events & EPOLLOUT) {
            DCHECK(_pollfds[fd].t_out == nullptr)
                << "BUG: fd: " << fd << " from " << t << " but " << _pollfds[fd].t_out;
            _pollfds[fd].t_out = t;
            _pollfds[fd].p_out = &fds[i];
            _pollfds[fd].events |= EPOLLOUT;
        }

        ev.events = _pollfds[fd].events;

        if (saved_events == 0) {
            throw_if(_efd.add(fd, ev) == -1);
        } else if (saved_events != _pollfds[fd].events) {
            throw_if(_efd.modify(fd, ev) == -1);
        }
        ++_npollfds;
    }
}

int io::remove_pollfds(pollfd *fds, nfds_t nfds) {
    int rvalue = 0;
    for (nfds_t i=0; i<nfds; ++i) {
        int fd = fds[i].fd;
        if (fds[i].revents) rvalue++;

        if (_pollfds[fd].p_in == &fds[i]) {
            _pollfds[fd].t_in.reset();
            _pollfds[fd].p_in = nullptr;
            _pollfds[fd].events ^= EPOLLIN;
        }

        if (_pollfds[fd].p_out == &fds[i]) {
            _pollfds[fd].t_out.reset();
            _pollfds[fd].p_out = nullptr;
            _pollfds[fd].events ^= EPOLLOUT;
        }

        if (_pollfds[fd].events == 0) {
            _efd.remove(fd);
        } else {
            epoll_event ev{};
            ev.data.fd = fd;
            ev.events = _pollfds[fd].events;
            throw_if(_efd.modify(fd, ev) == -1);
        }
        --_npollfds;
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
    // only process 100 events each iteration to keep it fair
    _events.resize(100);
    int ms = -1;
    auto now = this_ctx->scheduler.cached_time();
    if (when && *when > now) {
        // TODO: allow more than millisecond resolution?
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(*when - now).count();
        if (ms) {
            struct itimerspec tspec{};
            tspec.it_value.tv_sec = ms / 1000;
            tspec.it_value.tv_nsec = (ms % 1000) * 1000000;
            _tfd.settime(tspec);
            // we use timer_fd to break from epoll_wait
            // because its timeout isn't very accurate
            ms = -1;
        }
    } else {
        ms = 0;
    }

    _efd.wait(_events, ms);
    ptr<task::pimpl> t;
    for (auto &event : _events) {
        // NOTE: epoll will also return EPOLLERR and EPOLLHUP for every fd
        // even if they arent asked for, so we must wake up the tasks on any event
        // to avoid just spinning in epoll.
        int fd = event.data.fd;
        if (fd == _evfd.fd) {
            // our wake up eventfd was written to
            // clear events by reading value
            _evfd.read();
        } else if (fd == _tfd.fd) {
            // timerfd fired for sleeping/timeout tasks
        } else if ((size_t)fd < _pollfds.size()) {
            if (_pollfds[fd].t_in) {
                _pollfds[fd].p_in->revents = event.events;
                t = _pollfds[fd].t_in;
                DVLOG(5) << "IN EVENT on task: " << t;
                t->ready_for_io();
            }

            // check to see if pollout is a different task than pollin
            if (_pollfds[fd].t_out && _pollfds[fd].t_out != _pollfds[fd].t_in) {
                _pollfds[fd].p_out->revents = event.events;
                t = _pollfds[fd].t_out;
                DVLOG(5) << "OUT EVENT on task: " << t;
                t->ready_for_io();
            }

            if (_pollfds[fd].t_in == nullptr && _pollfds[fd].t_out == nullptr) {
                // TODO: otherwise we might want to remove fd from epoll
                LOG(ERROR) << "event " << event.events << " for fd: "
                    << event.data.fd << " but has no task";
            }
        } else {
            LOG(ERROR) << "BUG: mystery fd " << fd;
        }
    }
}



} // ten
