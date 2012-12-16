#include "ten/task/runtime.hh"

namespace ten {

namespace {
static std::mutex runtime_mutex;
static std::vector<runtime *> runtimes;
static std::atomic<uint64_t> runtime_count{0};
static task *waiting_task = nullptr;

static void add_runtime(runtime *r) {
    using namespace std;
    lock_guard<mutex> lock(runtime_mutex);
    runtimes.push_back(r);
    ++runtime_count;
}

static void remove_runtime(runtime *r) {
    using namespace std;
    lock_guard<mutex> lock(runtime_mutex);
    auto i = find(begin(runtimes), end(runtimes), r);
    if (i != end(runtimes)) {
        runtimes.erase(i);
        --runtime_count;
    }

    if (runtime_count == 1 && waiting_task) {
        waiting_task->ready();
    }
}

}

task *runtime::current_task() {
    return thread_local_ptr<runtime>()->_current_task;
}

runtime::runtime() : _canceled{false} {
    // TODO: reenable this when our libstdc++ is fixed
    //static_assert(clock::is_steady, "clock not steady");
    update_cached_time();
    _task._runtime = this;
    _current_task = &_task;
    add_runtime(this);
}

runtime::~runtime() {
    remove_runtime(this);
}

void runtime::cancel() {
    _canceled = true;
}

void runtime::shutdown() {
    using namespace std;
    lock_guard<mutex> lock(runtime_mutex);
    for (runtime *r : runtimes) {
        r->cancel();
    }
}

void runtime::wait_for_all() {
    using namespace std;
    CHECK(is_main_thread());
    runtime *r = thread_local_ptr<runtime>();
    {
        lock_guard<mutex> lock(runtime_mutex);
        waiting_task = r->_current_task;
        DVLOG(5) << "waiting for all " << waiting_task;
    }

    while (!r->_alltasks.empty() || runtime_count > 1) {
        r->schedule();
    }

    {
        lock_guard<mutex> lock(runtime_mutex);
        DVLOG(5) << "done waiting for all";
        waiting_task = nullptr;
    }
}


// XXX: only safe to call from debugger
int runtime::dump_all() {
    using namespace std;
    lock_guard<mutex> lock(runtime_mutex);
    for (runtime *r : runtimes) {
        LOG(INFO) << "runtime: " << r;
        r->dump();
    }
    return 0;
}

int runtime::dump() {
    LOG(INFO) << &_task;
#ifdef TEN_TASK_TRACE
    LOG(INFO) << _task._trace.str();
#endif
    for (shared_task &t : _alltasks) {
        LOG(INFO) << t.get();
#ifdef TEN_TASK_TRACE
        LOG(INFO) << t->_trace.str();
#endif
    }
    return 0;
}

void runtime::ready(task *t) {
    if (t->_ready.exchange(true) == false) {
        if (this != thread_local_ptr<runtime>()) {
            _dirtyq.push(t);
            // TODO: speed this up?
            std::unique_lock<std::mutex> lock{_mutex};
            _cv.notify_one();
        } else {
            DVLOG(5) << "readyq runtime ready: " << t;
            _readyq.push_back(t);
        }
    }
}

void runtime::remove_task(task *t) {
    DVLOG(5) << "remove task " << t;
    using namespace std;
    {
        // TODO: might not need this anymore
        // assert not in readyq
        auto i = find(begin(_readyq), end(_readyq), t);
        if (i != end(_readyq)) {
            _readyq.erase(i);
        }
    }

    // TODO: needed?
    //_alarms.remove(t);

    auto i = find_if(begin(_alltasks), end(_alltasks),
            [t](shared_task &other) {
            return other.get() == t;
            });
    _gctasks.push_back(*i);
    _alltasks.erase(i);
}

void runtime::check_dirty_queue() {
    task *t = nullptr;
    while (_dirtyq.pop(t)) {
        DVLOG(5) << "readyq adding " << t << " from dirtyq";
        _readyq.push_back(t);
    }
}

void runtime::check_timeout_tasks() {
    _alarms.tick(_now, [this](task *t, std::exception_ptr exception) {
        if (exception != nullptr && t->_exception == nullptr) {
            DVLOG(5) << "alarm with exception fired: " << t;
            t->_exception = exception;
        }
        ready(t);
    });
}

void runtime::schedule() {
    //CHECK(!_alltasks.empty());
    task *self = _current_task;

    if (_canceled && !_tasks_canceled) {
        // TODO: maybe if canceled is set we don't let any more tasks spawn?
        _tasks_canceled = true;
        for (auto t : _alltasks) {
            t->cancel();
        }
    }

    do {
        check_dirty_queue();
        update_cached_time();
        check_timeout_tasks();

        if (_readyq.empty()) {
            if (_alltasks.empty()) {
                std::lock_guard<std::mutex> lock(runtime_mutex);
                if (waiting_task && this == waiting_task->_runtime) {
                    waiting_task->ready();
                    continue;
                }
            }
            std::unique_lock<std::mutex> lock{_mutex};
            check_dirty_queue();
            if (!_readyq.empty()) {
                continue;
            }
            auto when = _alarms.when();
            if (when) {
                _cv.wait_until(lock, *when);
            } else {
                _cv.wait(lock);
            }
        }
    } while (_readyq.empty());

    using ::operator <<;
    DVLOG(5) << "readyq: " << _readyq;

    task *t = _readyq.front();
    _readyq.pop_front();
    t->_ready.store(false);
    _current_task = t;
    DVLOG(5) << self << " swap to " << t;
#ifdef TEN_TASK_TRACE
    self->_trace.capture();
#endif
    if (t != self) {
        self->_ctx.swap(t->_ctx, reinterpret_cast<intptr_t>(t));
    }
    _current_task = self;
    _gctasks.clear();
}

deadline::deadline(std::chrono::milliseconds ms) {
    if (ms.count() < 0)
        throw errorx("negative deadline: %jdms", intmax_t(ms.count()));
    if (ms.count() > 0) {
        runtime *r = thread_local_ptr<runtime>();
        task *t = r->_current_task;
        _alarm = std::move(runtime::alarm_clock::scoped_alarm(
                    r->_alarms, t, ms+runtime::now(), deadline_reached()
                    )
                );
        DVLOG(1) << "deadline alarm armed: " << _alarm._armed << " in " << ms.count() << "ms";
    }
}

void deadline::cancel() {
    _alarm.cancel();
}

deadline::~deadline() {
    cancel();
}

std::chrono::milliseconds deadline::remaining() const {
    return _alarm.remaining();
}

} // ten

