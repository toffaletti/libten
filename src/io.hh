#ifndef LIBTEN_IO_HH
#define LIBTEN_IO_HH

#include "task_impl.hh"
#include "ten/descriptors.hh"

namespace ten {

struct io {
    struct task_poll_state {
        ptr<task::pimpl> t_in = nullptr; // POLLIN task
        pollfd *p_in = nullptr; // pointer to pollfd structure that is on the task's stack
        ptr<task::pimpl> t_out = nullptr; // POLLOUT task
        pollfd *p_out = nullptr; // pointer to pollfd structure that is on the task's stack
        uint32_t events = 0; // events this fd is registered for
    };
    typedef std::vector<task_poll_state> poll_task_array;
    typedef std::vector<epoll_event> event_vector;

    //! array of tasks waiting on fds, indexed by the fd for speedy lookup
    poll_task_array pollfds;
    //! epoll events
    event_vector events;
    //! using for breaking out of epoll
    event_fd _evfd;
    //! timerfd used for breaking epoll_wait for timeouts
    // required because the timeout value for epoll_wait is not accurate
    timer_fd tfd;
    //! the epoll fd used for io in this runner
    epoll_fd efd;
    //! number of fds we've been asked to wait on
    size_t npollfds;

    io();
    void add_pollfds(ptr<task::pimpl> t, pollfd *fds, nfds_t nfds);
    int remove_pollfds(pollfd *fds, nfds_t nfds);
    bool fdwait(int fd, int rw, optional_timeout ms);
    int poll(pollfd *fds, nfds_t nfds, optional_timeout ms);
    void wakeup();
    void wait(optional<proc_time_t> when);
};

} // end namespace ten

#endif // LIBTEN_IO_HH
