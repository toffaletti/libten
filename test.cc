#include "timerspec.hh"
#include "reactor.hh"
#include <iostream>
#include <assert.h>
#include <utility>
#include <vector>

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

    timer_events(reactor &r) {
        r.add_hook(boost::bind(&timer_events::hook, this));
        r.add(timer.fd, EPOLLIN, boost::bind(&timer_events::callback, this, _1));
    }

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

std::vector<socket_fd> clients;
std::ostream &operator << (std::ostream &out, const std::vector<socket_fd> &v) {
    out << "[";
    for (std::vector<socket_fd>::const_iterator it = v.begin(); it!=v.end(); it++) {
        out << *it << ",";
    }
    out << "]";
    return out;
}

bool client_cb(uint32_t events, int fd, reactor &r) {
    std::cout << "client cb: " << fd << "\n";
    std::vector<socket_fd>::iterator it = std::find(clients.begin(), clients.end(), fd);
    if (it == clients.end()) {
        // this should never happen
        std::cout << "fd: " << fd << " not found in clients list\n";
        return false;
    }
    char buf[4];
    ssize_t rb;
    do {
        rb = it->read(buf, sizeof(buf));
        std::cout << "read " << rb << " bytes from " << *it << "\n";
    } while (rb == sizeof(buf));

    if (rb == 0) {
        std::cout << "erasing client: " << *it << "\n";
        clients.erase(it);
        std::cout << "clients: " << clients << "\n";

        //timer_fd *leak = new timer_fd();
        //itimerspec ts;
        //itimerspec prev;
        //memset(&ts, 0, sizeof(ts));
        //ts.it_value.tv_sec = 1;
        //ts.it_value.tv_nsec = 50;
        //leak->settime(0, ts, prev);
        //r.add(leak->fd, EPOLLIN, boost::bind(timer_cb, _1, boost::ref(*leak)));
    }

    return true;
}

bool accept_cb(uint32_t events, socket_fd &s, reactor &r) {
    address client_addr;
    socket_fd cs = s.accept(client_addr, SOCK_NONBLOCK);
    std::cout << "accepted " << client_addr << "\n";
    std::cout << "client socket: " << cs << "\n";
    r.add(cs.fd, EPOLLIN, boost::bind(client_cb, _1, cs.fd, boost::ref(r)));
    clients.push_back(std::move(cs));
    std::cout << "client socket: " << cs << "\n";
    std::cout << clients.size() << " clients\n";
    return true;
}

void timer_void(int n) {
    std::cout << "timer_void(" << n << ")\n";
}

int main(int argc, char *argv[]) {

#ifdef __GXX_EXPERIMENTAL_CXX0X__
    //fd_base fd1(0);
    //fd_base fd2(std::move(fd1));
    //assert(fd1.fd == -1);
    //assert(fd2.fd != -1);

    std::pair<socket_fd, socket_fd> sp(socket_fd::pair(AF_UNIX, SOCK_STREAM));
    {
        char a = 1;
        assert(sp.first.write(&a, 1) == 1);
        assert(sp.second.read(&a, 1) == 1);
        std::cout << "read: " << (int)a << " from socketpair\n";
    }
#endif

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
    r.add(s.fd, EPOLLIN, boost::bind(accept_cb, _1, boost::ref(s), boost::ref(r)));
    r.run();

    return 0;
}

