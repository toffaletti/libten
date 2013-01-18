#ifndef TEN_TASK_IO_HH
#define TEN_TASK_IO_HH

#include "ten/descriptors.hh"
#include "ten/optional.hh"
#include <thread>
#include <mutex>
#include <poll.h>
#include "ten/task/runtime.hh"

namespace ten {

class io {
private:
    struct task_poll_state {
        task_pimpl *task;
        pollfd *pfd;

        task_poll_state(task_pimpl *task_, pollfd *pfd_)
            : task(task_), pfd(pfd_) {}
    };
    struct fd_poll_state {
        std::deque<task_poll_state> in;
        std::deque<task_poll_state> out;
        uint32_t events = 0; // events this fd is registered for
    };
    typedef std::vector<fd_poll_state> fd_array;

    epoll_fd _efd;
    event_fd _evfd;
    timer_fd _tfd;
    //! number of fds we've been asked to wait on
    size_t npollfds;
    //! array of tasks waiting on fds, indexed by the fd for speedy lookup
    fd_array pollfds;

    std::vector<epoll_event> _events;

    void add_pollfds(task_pimpl *t, pollfd *fds, nfds_t nfds);
    int remove_pollfds(pollfd *fds, nfds_t nfds);
public:
    io();
    ~io();

    void wakeup() {
        _evfd.write(0);
    }

    int poll(pollfd *fds, nfds_t nfds, optional_timeout ms);
    bool fdwait(int fd, int rw, optional_timeout ms);

    void wait(optional<runtime::time_point> when);
};

} // ten

#endif
