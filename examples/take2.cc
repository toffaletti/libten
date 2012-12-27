#include "ten/logging.hh"
#include "../src/context.hh"
#include "ten/descriptors.hh"

#include <memory>
#include <thread>
#include <atomic>

#include "t2/channel.hh"

#include <sys/syscall.h> // gettid
#include <sys/stat.h> // umask
#include <signal.h> // sigaction

using namespace ten;
using namespace t2;

static void runtime_init() {
    // allow log files and message queues to be created group writable
    umask(0);
    InitGoogleLogging(program_invocation_short_name);
    InstallFailureSignalHandler();
    FLAGS_logtostderr = true;


    CHECK(getpid() == syscall(SYS_gettid)) << "must call in main thread before anything else";
    //ncpu_ = sysconf(_SC_NPROCESSORS_ONLN);
    stack_t ss;
    ss.ss_sp = calloc(1, SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    PCHECK(sigaltstack(&ss, NULL) == 0) << "setting signal stack failed";

    struct sigaction act;

    // ignore SIGPIPE
    memset(&act, 0, sizeof(act));
    PCHECK(sigaction(SIGPIPE, NULL, &act) == 0) << "getting sigpipe handler failed";
    if (act.sa_handler == SIG_DFL) {
        act.sa_handler = SIG_IGN;
        PCHECK(sigaction(SIGPIPE, &act, NULL) == 0) << "setting sigpipe handler failed";
    }

    //netinit();
}

static std::atomic<uint64_t> taskcount{0};

struct tasklet;

struct thread_data {
    context ctx;
    std::unique_ptr<tasklet> self;
};

__thread thread_data *tld = nullptr;

struct tasklet {

    context _ctx;
    std::function<void ()> _f;

    tasklet(std::function<void ()> &&f)
        : _ctx(tasklet::trampoline),
        _f(f)
    {
        ++taskcount;
    }

    ~tasklet() {
        --taskcount;
    }

    static void yield() {
        tld->self->_yield();
    }

    void _yield() {
        _ctx.swap(tld->ctx, 0);
    }

    bool swap() {
        if (_f) {
            tld->ctx.swap(_ctx, reinterpret_cast<intptr_t>(this));
        }
        return static_cast<bool>(_f);
    }

    static void trampoline(intptr_t arg) noexcept;
};

void tasklet::trampoline(intptr_t arg) noexcept {
    tasklet *self = reinterpret_cast<tasklet *>(arg);
    self->_f();
    self->_f = nullptr;
    self->_ctx.swap(tld->ctx, 0);
    LOG(FATAL) << "Oh no! You fell through the trampoline " << self;
}

struct Foo {
    int value;

    Foo(const Foo &) = delete;

    Foo(Foo &&other) : value(0) {
        std::swap(value, other.value);
    }

    Foo &operator= (Foo &&other) {
        std::swap(value, other.value);
        return *this;
    }

    //Foo(const Foo &other) {
    //    LOG(INFO) << "copy ctor";
    //    value = other.value;
    //}

    Foo(int v) : value(v) {}
};

static void recver(channel<Foo> chan) {
    optional<Foo> v;
    for (;;) {
        optional<Foo> tmp = chan.recv();
        if (!tmp) break;
        v = std::move(tmp);
    }
    if (v) {
        LOG(INFO) << "last value: " << v->value;
    }
}

static void counter(int n) {
    for (int i=0; i<100; ++i) {
        LOG(INFO) << n << " i: " << i;
        tasklet::yield();
        if (i == 5) {
            LOG(INFO) << "sleepy time";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    LOG(INFO) << "end counter " << n;
}

static channel<std::unique_ptr<tasklet>> sched_chan;
static channel<std::tuple<int, std::unique_ptr<tasklet>>> io_chan;
static epoll_fd efd;
static event_fd evfd;

static void net_init() {
    epoll_event ev{EPOLLIN};
    ev.data.fd = evfd.fd;
    efd.add(evfd.fd, ev);
}

static void accepter(socket_fd fd) {
    LOG(INFO) << "in accepter";
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.fd = fd.fd;
    efd.add(fd.fd, ev);
    for (;;) {
        LOG(INFO) << "sending self to io chan";
        {
            tasklet *tmp = tld->self.get(); 
            io_chan.send(std::make_pair(fd.fd, std::move(tld->self)));
            tmp->_yield();
        }
        LOG(INFO) << "after wait";
        address addr;
        int nfd = fd.accept(addr);
        LOG(INFO) << "accepted: " << nfd;
        close(nfd);

        efd.modify(fd.fd, ev);
    }
}

static void scheduler() {
    thread_data td;
    tld = &td;
    while (taskcount > 0) {
        auto t = sched_chan.recv();
        if (!t) break;
        tld->self = std::move(*t);
        if (tld->self->swap()) {
            if (tld->self) {
                LOG(INFO) << "scheduling " << tld->self.get();
                sched_chan.send(std::move(tld->self));
            }
        }
    }
    LOG(INFO) << "exiting scheduler";
    sched_chan.close();
}

static void io_scheduler() {
    std::vector<std::unique_ptr<tasklet>> io_tasks;
    std::vector<epoll_event> events;
    events.reserve(1000);
    for (;;) {
        events.resize(1000);
        efd.wait(events, -1);
        auto new_tasks = io_chan.recv_all();
        for (auto &tup : new_tasks) {
            int fd = std::get<0>(tup);
            std::unique_ptr<tasklet> t = std::move(std::get<1>(tup));
            if ((size_t)fd > io_tasks.size()) {
                io_tasks.resize(fd+1);
            }
            io_tasks[fd] = std::move(t);
        }

        for (epoll_event &ev : events) {
            int fd = ev.data.fd;
            if (fd == evfd.fd) {
                if (evfd.read() == 7) return;
                continue;
            }
            // TODO: send them all in one go
            CHECK((size_t)fd < io_tasks.size());
            sched_chan.send(std::move(io_tasks[fd]));
        }
    }
}

int main() {
    runtime_init();
    net_init();
    LOG(INFO) << "take2";

    std::thread io_thread(io_scheduler);

#if 0
    for (int i=0; i<2; ++i) {
        std::unique_ptr<tasklet> t(new tasklet(std::bind(counter, i)));
        sched_chan.send(std::move(t));
    }

#endif
    socket_fd listen_fd{AF_INET, SOCK_STREAM};
    address addr{"0.0.0.0", 7700};
    listen_fd.bind(addr);
    LOG(INFO) << "bound to " << addr;
    listen_fd.listen();
    std::unique_ptr<tasklet> t(new tasklet(std::bind(accepter, dup(listen_fd.fd))));
    sched_chan.send(std::move(t));

    std::vector<std::thread> schedulers;
    for (int i=0; i<6; ++i) {
        schedulers.emplace_back(scheduler);
    }

    LOG(INFO) << "joining";

    for (auto &sched : schedulers) {
        sched.join();
    }

    evfd.write(7);
    io_thread.join();
}

