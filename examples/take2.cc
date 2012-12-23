#include "ten/logging.hh"
#include "ten/optional.hh"
#include "../src/context.hh"

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <queue>
#include <atomic>

#include <sys/syscall.h> // gettid
#include <sys/stat.h> // umask
#include <signal.h> // sigaction

using namespace ten;

template <typename T>
class channel {
private:
    struct pimpl;
    std::shared_ptr<pimpl> _pimpl;
    bool _autoclose;
public:
    channel(bool autoclose=false);
    ~channel();
    void close() noexcept;

    bool send(const T &t) {
        std::lock_guard<std::mutex> lock(_pimpl->mutex);
        if (_pimpl->closed) return false;
        _pimpl->queue.push_back(std::forward<T>(t));
        _pimpl->not_empty.notify_one();
        return true;
    }

    bool send(T &&t) {
        std::lock_guard<std::mutex> lock(_pimpl->mutex);
        if (_pimpl->closed) return false;
        _pimpl->queue.push_back(std::forward<T>(t));
        _pimpl->not_empty.notify_one();
        return true;
    }

    template <class ...Args>
        bool emplace_send(Args&& ...args) {
            std::lock_guard<std::mutex> lock(_pimpl->mutex);
            if (_pimpl->closed) return false;
            _pimpl->queue.emplace_back(std::forward<Args>(args)...);
            _pimpl->not_empty.notify_one();
            return true;
        }

    optional<T> recv() {
        std::unique_lock<std::mutex> lock(_pimpl->mutex);
        while (_pimpl->queue.empty() && !_pimpl->closed) {
            _pimpl->not_empty.wait(lock);
        }
        if (_pimpl->queue.empty()) return {};
        optional<T> t(std::move(_pimpl->queue.front()));
        _pimpl->queue.pop_front();
        return t;
    }
};

template <typename T>
struct channel<T>::pimpl {
    std::deque<T> queue;
    std::mutex mutex;
    std::condition_variable not_empty;
    bool closed = false;
};

template <typename T>
channel<T>::channel(bool autoclose)
    : _autoclose(autoclose)
{
    _pimpl = std::make_shared<pimpl>();
}

template <typename T>
channel<T>::~channel() {
    if (_autoclose) close();
}

template <typename T>
void channel<T>::close() noexcept {
    std::lock_guard<std::mutex> lock(_pimpl->mutex);
    if (!_pimpl->closed) {
        _pimpl->closed = true;
        _pimpl->not_empty.notify_all();
    }
}

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

struct tasklet {
    static __thread context *_from;
    static __thread context *_self;

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
        _self->swap(*_from, 0);
    }

    bool swap(context &from) {
        if (_f) {
            _from = &from;
            _self = &_ctx;
            from.swap(_ctx, reinterpret_cast<intptr_t>(this));
        }
        return static_cast<bool>(_f);
    }

    static void trampoline(intptr_t arg) noexcept;
};

__thread context *tasklet::_from = nullptr;
__thread context *tasklet::_self = nullptr;

void tasklet::trampoline(intptr_t arg) noexcept {
    tasklet *self = reinterpret_cast<tasklet *>(arg);
    self->_f();
    self->_f = nullptr;
    self->_ctx.swap(*_from, 0);
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

static void scheduler(channel<std::unique_ptr<tasklet>> chan) {
    context ctx;
    while (taskcount > 0) {
        auto t = chan.recv();
        if (!t) break;
        if ((*t)->swap(ctx)) {
            chan.send(std::move(*t));
        }
    }
    chan.close();
    LOG(INFO) << "exiting scheduler";
}

int main() {
    runtime_init();
    LOG(INFO) << "take2";

    channel<std::unique_ptr<tasklet>> chan;

    for (int i=0; i<2; ++i) {
        std::unique_ptr<tasklet> t(new tasklet(std::bind(counter, i)));
        chan.send(std::move(t));
    }

    std::vector<std::thread> schedulers;
    for (int i=0; i<6; ++i) {
        schedulers.emplace_back(std::bind(scheduler, chan));
    }

    for (auto &sched : schedulers) {
        sched.join();
    }
}

