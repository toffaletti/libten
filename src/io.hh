#ifndef LIBTEN_IO_HH
#define LIBTEN_IO_HH

#include "task_impl.hh"
#include "ten/descriptors.hh"

namespace ten {

class io {
private:
    struct task_poll_state {
        ptr<task::pimpl> t;
        pollfd *pfd;

        task_poll_state(ptr<task::pimpl> t_, pollfd *pfd_)
            : t{t_}, pfd{pfd_} {}
    };

    struct fd_poll_state {
        std::vector<task_poll_state> tasks;
        uint32_t events = 0; // events this fd is registered for
    };

    typedef std::vector<fd_poll_state> fd_array;
    typedef std::vector<epoll_event> event_vector;
private:
    //! array of tasks waiting on fds, indexed by the fd for speedy lookup
    fd_array _pollfds;
    //! epoll events
    event_vector _events;
    //! using for breaking out of epoll
    event_fd _evfd;
    //! timerfd used for breaking epoll_wait for timeouts
    // required because the timeout value for epoll_wait is not accurate
    timer_fd _tfd;
    //! the epoll fd used for io in this runner
    epoll_fd _efd;
    //! number of fds we've been asked to wait on
    size_t _npollfds = 0;
private:
    void add_pollfds(ptr<task::pimpl> t, pollfd *fds, nfds_t nfds);
    int remove_pollfds(pollfd *fds, nfds_t nfds);
public:
    io();

    bool fdwait(int fd, int rw, optional_timeout ms);
    int poll(pollfd *fds, nfds_t nfds, optional_timeout ms);

    void wakeup();
    void wait(optional<proc_time_t> when);
};

} // end namespace ten

#endif // LIBTEN_IO_HH
