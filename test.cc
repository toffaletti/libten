#include "descriptors.hh"
#include <iostream>
#include <assert.h>
#include <utility>
#include <vector>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

class reactor {
public:
    typedef boost::function<bool (uint32_t events)> event_func;
    struct state {
        state(int fd_, const event_func &func_) : fd(fd_), func(func_) {}
        event_func func;
        int fd;
        bool operator == (const state &other) { return fd == other.fd; }
    };
    typedef boost::ptr_vector<state> state_vector;
    typedef std::vector<epoll_event> event_vector;


    reactor() : done(false) {}

    void add(int fd, int events, const event_func &f) {
        thunk.push_back(new state(fd, f));
        epoll_event ev;
        ev.events = events;
        ev.data.ptr = &thunk.back();
        assert(e.add(fd, ev) == 0);
    }

    void remove(int fd) {
        state_vector::iterator it = std::find(thunk.begin(), thunk.end(), state(fd, NULL));
        if (it != thunk.end()) {
            thunk.erase(it);
            assert(e.remove(fd) == 0);
        }
    }

    void runonce() {
        event_vector events;
        e.wait(events);
        std::cout << events.size() << " events triggered\n";
        for (event_vector::const_iterator it=events.begin(); it!=events.end(); ++it) {
            state *s = (state *)it->data.ptr;
            try {
                if (!s->func(it->events)) {
                    remove(s->fd);
                }
            } catch (std::exception &ex) {
                // TODO: replace with real logging
                std::cerr << "exception caught at " << __FILE__ << ":" << __LINE__ << " " << ex.what() << std::endl;
            }
        }
    }

    void run() {
        while (!done) {
            runonce();
        }
    }

    void quit() { done = true; }

protected:
    epoll_fd e;
    state_vector thunk;
    bool done;
};

bool timer_cb(uint32_t events, timer_fd &t) {
    uint64_t fired;
    assert(t.read(&fired, sizeof(fired)) == 8);
    std::cout << "timer fired " << fired << " times\n";
    return false;
}

bool sigint_cb(uint32_t events, signal_fd &s, reactor &r) {
    signalfd_siginfo siginfo;
    s.read(siginfo);
    std::cout << "got SIGINT from pid: " << siginfo.ssi_pid << "\n";
    r.quit();
    return true;
}

bool accept_cb(uint32_t events, socket_fd &s) {
    address client_addr;
    s.accept(client_addr);
    std::cout << "accepted " << client_addr << "\n";
    return true;
}

int main(int argc, char *argv[]) {

    //fd_base fd1(0);
    //fd_base fd2(std::move(fd1));
    //assert(fd1.fd == -1);
    //assert(fd2.fd != -1);

    epoll_fd efd;

    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    socket_fd sp1(sv[0]);
    socket_fd sp2(sv[1]);

    char a = 1;
    pipe_fd pipe;
    pipe.write(&a, 1);
    assert(pipe.read(&a, 1) == 1);
    printf("read a: %d\n", a);

    timer_fd t;
    itimerspec ts;
    itimerspec prev;
    memset(&ts, 0, sizeof(ts));
    ts.it_value.tv_sec = 1;
    ts.it_value.tv_nsec = 50;
    t.settime(0, ts, prev);

    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("0.0.0.0", 9900);
    std::cout << "binding to " << addr << "\n";
    s.bind(addr);
    std::cout << "binded to " << addr << "\n";
    s.listen();

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    signal_fd sig(sigset);

    reactor r;
    r.add(t.fd, EPOLLIN, boost::bind(timer_cb, _1, boost::ref(t)));
    r.add(sig.fd, EPOLLIN, boost::bind(sigint_cb, _1, boost::ref(sig), boost::ref(r)));
    r.add(s.fd, EPOLLIN, boost::bind(accept_cb, _1, boost::ref(s)));
    r.run();
    r.remove(s.fd);

    return 0;
}

