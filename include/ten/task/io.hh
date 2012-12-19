#ifndef TEN_TASK_IO_HH
#define TEN_TASK_IO_HH

#include "ten/descriptors.hh"
#include <thread>
#include <mutex>
#include <poll.h>

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
    std::thread _poll_thread;

    std::mutex _mutex;
    // TODO: go away
    //! number of fds we've been asked to wait on
    size_t npollfds;
    //! array of tasks waiting on fds, indexed by the fd for speedy lookup
    fd_array pollfds;

    io();

    enum class event {
        read,
        write,
        accept
    };

    void poll_proc();
    void add_pollfds(task_pimpl *t, pollfd *fds, nfds_t nfds);
    int remove_pollfds(pollfd *fds, nfds_t nfds);
public:
    static io &singleton();

    int poll(pollfd *fds, nfds_t nfds, uint64_t ms);
    bool fdwait(int fd, int rw, uint64_t ms);
    void wait_for_fd_events(int fd, uint32_t events);

    ~io();
};

} // ten

#endif
