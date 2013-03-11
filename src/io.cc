#include "io.hh"
#include "thread_context.hh"

namespace ten {

#ifdef HAS_CARES
extern inotify_fd resolv_conf_watch_fd;
#endif // HAS_CARES

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

#ifdef HAS_CARES
    // add inotify_fd used to watch /etc/resolv.conf
    {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        int e_fd = resolv_conf_watch_fd.fd;
        ev.data.fd = e_fd;
        throw_if(_efd.add(e_fd, ev) == -1);
    }
#endif // HAS_CARES
}

void io::add_pollfds(ptr<task::impl> t, pollfd *fds, nfds_t nfds) {
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

        _pollfds[fd].tasks.emplace_back(t, &fds[i]);
        _pollfds[fd].events |= fds[i].events;

        ev.events = _pollfds[fd].events | EPOLLONESHOT;

        if (saved_events == 0) {
            int status = _efd.modify(fd, ev);
            if (status == -1 && errno == ENOENT) {
                throw_if(_efd.add(fd, ev) == -1);
            } else {
                throw_if(status == -1);
            }
        } else if (saved_events != _pollfds[fd].events) {
            throw_if(_efd.modify(fd, ev) == -1);
        }
        ++_npollfds;
    }
}

int io::remove_pollfds(pollfd *fds, nfds_t nfds) {
    using namespace std;
    int evented_fds = 0;
    for (nfds_t i=0; i<nfds; ++i) {
        int fd = fds[i].fd;

        uint32_t saved_events = _pollfds[fd].events;

        auto it = find_if(begin(_pollfds[fd].tasks), end(_pollfds[fd].tasks),
                [&](const task_poll_state &st) { return st.pfd == &fds[i]; });
        if (it != end(_pollfds[fd].tasks)) {
            _pollfds[fd].tasks.erase(it);
            // calculate new event mask
            uint32_t events = 0;
            for (auto &st : _pollfds[fd].tasks) {
                events |= st.pfd->events;
            }
            _pollfds[fd].events = events;
        }

        if (fds[i].revents) {
            ++evented_fds;
        } else {
            if (_pollfds[fd].events == 0) {
                _efd.remove(fd);
            } else if (saved_events != _pollfds[fd].events) {
                epoll_event ev{};
                ev.data.fd = fd;
                ev.events = _pollfds[fd].events;
                throw_if(_efd.modify(fd, ev) == -1);
            }
        }
        --_npollfds;
    }
    return evented_fds;
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
    const auto t = kernel::current_task();
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
            auto now = kernel::now();
            timeout_alarm.emplace(this_ctx->scheduler.arm_alarm(t, now+*ms));
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
    if (when) {
        auto now = kernel::now();
        if (*when > now) {
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
            // don't wait at all
            ms = 0;
        }
    }

    _efd.wait(_events, ms);
    ptr<task::impl> t;
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
#ifdef HAS_CARES
        } else if (fd == resolv_conf_watch_fd.fd) {
            inotify_event event;
            // this read should only succeed in one thread
            // but all threads should get the event
            resolv_conf_watch_fd.read(event);
            // force next dns query to re-ares_init
            this_ctx->dns_channel.reset();
#endif // HAS_CARES
        } else if ((size_t)fd < _pollfds.size()) {
            for (auto &st : _pollfds[fd].tasks) {
                if ((st.pfd->events & event.events) ||
                        event.events & (EPOLLERR | EPOLLHUP))
                {
                    st.pfd->revents = event.events;
                    DVLOG(5) << "fd " << fd << " EVENTS: " << event.events << " on task: " << st.t;
                    st.t->ready_for_io();
                }
            }

            if (_pollfds[fd].tasks.empty()) {
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
