#include "descriptors.hh"
#include <iostream>
#include <assert.h>
#include <utility>
#include <vector>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

class reactor;

std::ostream &operator << (std::ostream &out, const timespec &ts) {
    out << "{" << ts.tv_sec << "," << ts.tv_nsec << "}";
    return out;
}

bool operator == (const timespec &a, const timespec &b) {
    return memcmp(&a, &b, sizeof(timespec)) == 0;
}

bool operator > (const timespec &a, const timespec &b) {
    if (a.tv_sec > b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec > b.tv_nsec) return true;
    }
    return false;
}

bool operator < (const timespec &a, const timespec &b) {
    if (a.tv_sec < b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec < b.tv_nsec) return true;
    }
    return false;
}

bool operator <= (const timespec &a, const timespec &b) {
    if (a.tv_sec < b.tv_sec) return true;
    if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec <= b.tv_nsec) return true;
    }
    return false;
}

class timer_events {
public:
    typedef boost::function<void (void)> timer_func;
    struct state {
        state(const timespec &ts_, const timer_func &func_) : ts(ts_), func(func_) {}
        timespec ts;
        timer_func func;
    };
    struct state_comp {
        bool operator ()(const state &a, const state &b) const {
            return a.ts.tv_sec > b.ts.tv_sec;
        }
    };
    typedef boost::ptr_vector<state> state_vector;

    timer_events(reactor &r);

    // two types of timers single and periodic
    // for single shot, we can keep a heap and use one timerfd
    // set to the shortest interval (top of the heap).
    // for periodic it is easier just to create a timerfd per.
    // there shouldn't be a lot of these.
    void add(const struct itimerspec &ts, const timer_func &func) {
        static struct timespec zero = {0, 0};
        timespec abs;
        if (ts.it_interval == zero) {
            // one shot
            // get the current time so we can order heap by abs time
            THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &abs));
            if (ts.it_value.tv_sec)
                abs.tv_sec += ts.it_value.tv_sec;
            // TODO: turn nsec overflow into sec ?
            if (ts.it_value.tv_nsec)
                abs.tv_nsec += ts.it_value.tv_nsec;
            else
                // we don't need nanosec resolution
                abs.tv_nsec = 0;
            heap.push_back(new state(abs, func));
        } else {
            // periodic
        }
    }

protected:
    state_vector heap;
    timer_fd timer;

    void hook() {
        itimerspec ts;
        itimerspec old;
        memset(&ts, 0, sizeof(ts));
        if (!heap.empty()) {
            std::make_heap(heap.begin(), heap.end(), state_comp());
            std::cout << "heap top: " << heap.front().ts << "\n";
            ts.it_value = heap.front().ts;
        }
        timer.settime(TFD_TIMER_ABSTIME, ts, old);
    }

    bool callback(uint32_t events) {
        std::cout << "callback()" << std::endl;
        uint64_t fired;
        assert(timer.read(&fired, sizeof(fired)) == 8);
        std::cout << "fired " << fired << " times\n";
        timespec now;
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));

        // TODO: store the end from when make_heap was called
        // might need to use that here?
        timespec ts = heap.front().ts;
        while (!heap.empty() && heap.front().ts <= now) {
            std::pop_heap(heap.begin(), heap.end(), state_comp());
            // trigger timer callback
            std::cout << "invoking timer callback\n";
            heap.back().func();
            // remove timer
            heap.pop_back();
        }
        return true;
    }


};

class reactor {
public:
    typedef boost::function<void (void)> hook_func;
    typedef std::vector<hook_func> hook_vector;
    typedef boost::function<bool (uint32_t events)> event_func;
    struct state {
        state(int fd_, const event_func &func_) : fd(fd_), func(func_) {}
        int fd;
        event_func func;
        bool operator == (const state &other) { return fd == other.fd; }
    };
    typedef boost::ptr_vector<state> state_vector;
    typedef std::vector<epoll_event> event_vector;

    reactor() : done(false) {}

    void add(int fd, int events, const event_func &f) {
        thunk.push_back(new state(fd, f));
        epoll_event ev;
        memset(&ev, 0, sizeof(ev));
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

    void add_hook(const hook_func &func) {
        hooks.push_back(func);
    }

    void call_hooks() {
        for (hook_vector::const_iterator it = hooks.begin(); it!=hooks.end(); it++) {
            (*it)();
        }
    }

    void run() {
        while (!done) {
            call_hooks();
            runonce();
        }
    }

    void quit() { done = true; }

protected:
    epoll_fd e;
    state_vector thunk;
    hook_vector hooks;
    bool done;
};

timer_events::timer_events(reactor &r) {
    r.add_hook(boost::bind(&timer_events::hook, this));
    r.add(timer.fd, EPOLLIN, boost::bind(&timer_events::callback, this, _1));
}

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

void timer_void(int n) {
    std::cout << "timer_void(" << n << ")\n";
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

    timer_events te(r);
    memset(&ts, 0, sizeof(ts));
    ts.it_value.tv_sec = 2;
    te.add(ts, boost::bind(timer_void, 2));
    ts.it_value.tv_sec = 1;
    te.add(ts, boost::bind(timer_void, 1));
    ts.it_value.tv_sec = 0;
    te.add(ts, boost::bind(timer_void, 0));

    ts.it_value.tv_sec = 1;
    te.add(ts, boost::bind(timer_void, 11));


    r.add(t.fd, EPOLLIN, boost::bind(timer_cb, _1, boost::ref(t)));
    r.add(sig.fd, EPOLLIN, boost::bind(sigint_cb, _1, boost::ref(sig), boost::ref(r)));
    r.add(s.fd, EPOLLIN, boost::bind(accept_cb, _1, boost::ref(s)));
    r.run();
    r.remove(s.fd);

    return 0;
}

