#include "descriptors.hh"
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

// TODO: replace with proper logging
#include <iostream>

class reactor : public boost::noncopyable {
public:
    typedef boost::function<void (void)> hook_func;
    typedef std::vector<hook_func> hook_vector;
    typedef boost::function<bool (uint32_t events)> event_func;
    struct state {
        state(int fd_, const event_func &func_) : fd(fd_), func(func_) {}
        int fd;
        event_func func;
        bool operator == (const state &other) { return fd == other.fd; }
        friend std::ostream &operator << (std::ostream &out, const state &s) {
            out << "state(" << s.fd << "," << (void *)&s.func << ")";
            return out;
        }
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

    void modify(int fd, int events) {
        state_vector::iterator it = std::find(thunk.begin(), thunk.end(), state(fd, NULL));
        if (it != thunk.end()) {
            epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.events = events;
            ev.data.ptr = &(*it);
            assert(e.modify(fd, ev) == 0);
        }
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
        std::cerr << events.size() << " events triggered\n";
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

