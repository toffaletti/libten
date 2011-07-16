#include <deque>
#include <list>
#include <boost/function.hpp>
#include "thread.hh"
#include "coroutine.hh"
#include "descriptors.hh"

namespace scheduler {
class coroutine;
class thread;

namespace detail {
extern __thread thread *thread_;
}
typedef std::deque<coroutine *> coro_deque;

class thread : boost::noncopyable {
public:
    typedef std::list<thread *> list;

    p::thread id() { return p::thread(t); }

    static thread *spawn(const coroutine::func_t &f) {
        return new thread(f);
    }

    static thread *self() {
        if (detail::thread_ == NULL) {
            detail::thread_ = new thread;
        }
        return detail::thread_;
    }

    void sleep() {
        mutex::scoped_lock l(mut);
        asleep = true;
        while (asleep) {
            cond.wait(l);
        }
    }

    void wakeup() {
        mutex::scoped_lock l(mut);
        wakeup_nolock();
    }

    void schedule(bool loop=true);

    static size_t count();

    static void poll(int fd, int events);

protected:
    friend class coroutine;

    // called by coroutine::spawn
    void add_to_runqueue(coroutine *c) {
        mutex::scoped_lock l(mut);
        runq.push_back(c);
        wakeup_nolock();
    }

    bool add_to_runqueue_if_asleep(coroutine *c) {
        mutex::scoped_lock l(mut);
        if (asleep) {
            runq.push_back(c);
            wakeup_nolock();
            return true;
        }
        return false;
    }

    void delete_from_runqueue(coroutine *c) {
        mutex::scoped_lock l(mut);
        assert(c == runq.back());
        runq.pop_back();
    }

    // lock must already be held
    void wakeup_nolock() {
        if (asleep) {
            asleep = false;
            cond.signal();
        }
    }

    static void add_to_empty_runqueue(coroutine *c);

    void set_coro(coroutine *c) { coro = c; }
    coroutine *get_coro() {
        return coro;
    }

private:
    static mutex tmutex;
    static list threads;

    p::thread t;

    mutex mut;
    condition cond;
    bool asleep;

    coroutine scheduler;
    coroutine *coro;
    coro_deque runq;
    epoll_fd efd;

    thread() : asleep(false), coro(0) {
        t=p::thread::self();
        //append_to_list();
    }

    thread(coroutine *c) : asleep(false), coro(0) {
        add_to_runqueue(c);
        p::thread::create(t, start, this);
    }

    thread(const coroutine::func_t &f) : asleep(false), coro(0) {
        coroutine *c = new coroutine(f);
        add_to_runqueue(c);
        p::thread::create(t, start, this);
    }

    ~thread() {
    }

    static void *start(void *arg);

    void append_to_list() {
        mutex::scoped_lock l(thread::tmutex);
        threads.push_back(this);
    }

    void remove_from_list() {
        mutex::scoped_lock l(thread::tmutex);
        threads.push_back(this);
    }
};

} // end namespace scheduler

