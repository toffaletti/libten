#include "scheduler.hh"
#include "thread_context.hh"
#include <sys/syscall.h>

namespace ten {

scheduler::scheduler()
  : _main_task{std::make_shared<task::pimpl>()}, 
    ctask{_main_task.get()},
    _taskcount{0},
    _canceled{false}
{
    _main_task->_scheduler.reset(this);
    update_cached_time();
}

scheduler::~scheduler() {
    // wait for all tasks to exit
    // this is needed because main task could be canceled
    // and exit before other tasks do.
    wait_for_all();
    DVLOG(5) << "scheduler freed: " << this;
}

void scheduler::shutdown() {
    if (!_shutdown_sequence_initiated) {
        _shutdown_sequence_initiated = true;
        for (auto &t : _alltasks) {
            // don't add ourself to the _readyq
            // TODO: this assumes the task that called this will
            // behave and exit itself. perhaps we should cancel it
            // however if it does exit and it canceled itself
            // it will be in the readyq and trigger the DCHECK in remove_task
            if (t.get() == ctask.get()) continue;
            t->cancel();
        }
        _main_task->cancel();
    }
}

io& scheduler::get_io() {
    if (!_io) {
        _io.emplace();
    }
    return *_io;
}

void scheduler::wait_for_all() {
    DVLOG(5) << "entering loop";
    _looping = true;
    while (_taskcount > 0) {
        schedule();
    }
    _looping = false;
    DVLOG(5) << "exiting loop";

    if (getpid() == syscall(SYS_gettid)) {
        // if the main proc is exiting we need to cancel
        // all other procs (threads) and wait for them
        this_ctx->cancel_all();
        // wait for all threads besides this one to exit
        while (thread_context::count() > 1) {
            // TODO: remove this busy loop in favor of sleeping the proc
            std::this_thread::sleep_for(std::chrono::milliseconds{60});
        }
    }
}

void scheduler::check_canceled() {
    if (_canceled) {
        shutdown();
    }
}

void scheduler::check_dirty_queue() {
    ptr<task::pimpl> t = nullptr;
    while (_dirtyq.pop(t)) {
        DVLOG(5) << "dirty readying " << t;
        _readyq.push_front(t);
    }
}

void scheduler::check_timeout_tasks() {
    update_cached_time();
    // wake up sleeping tasks
    _alarms.tick(_now, [this](ptr<task::pimpl> t, std::exception_ptr exception) {
        if (exception != nullptr && t->exception == nullptr) {
            DVLOG(5) << "alarm with exception fired: " << t;
            t->exception = exception;
        }
        DVLOG(5) << "TIMEOUT on task: " << t;
        t->ready_for_io();
    });
}

void scheduler::wait(std::unique_lock <std::mutex> &lock, optional<proc_time_t> when) {
    // do not wait if _readyq is not empty
    check_dirty_queue();
    if (!_readyq.empty()) return;
    if (_io) {
        lock.unlock();
        _io->wait(when);
        lock.lock();
    } else {
        if (when) {
            _cv.wait_until(lock, *when);
        } else {
            _cv.wait(lock);
        }
    }
}

void scheduler::schedule() {
    if (_looping && _taskcount == 0) {
        // XXX: ugly hack
        // this should resume in ::wait_for_all()
        // which should only be called from main task.
        // the purpose of this is to allow other tasks to exit
        // while the main task sticks around. then swap to
        // the main task with will exit the thread/process
        _main_task->ready();
    }
    ptr<task::pimpl> saved_task = ctask;
    try {
        do {
            check_canceled();
            check_dirty_queue();
            check_timeout_tasks();
            if (_readyq.empty()) {
                auto when = _alarms.when();
                std::unique_lock<std::mutex> lock{_mutex};
                wait(lock, when);
            }
        } while (_readyq.empty());
        ptr<task::pimpl> t{_readyq.front()};
        _readyq.pop_front();
        DCHECK(t->is_ready);
        t->is_ready.store(false);
        ctask = t;
        DVLOG(5) << this << " swapping to: " << t;
#ifdef TEN_TASK_TRACE
        saved_task->_trace.capture();
#endif
        if (t != saved_task) {
            saved_task->_ctx.swap(t->_ctx, reinterpret_cast<intptr_t>(t.get()));
        }
        ctask = saved_task;
        _gctasks.clear();
    } catch (backtrace_exception &e) {
        LOG(FATAL) << e.what() << "\n" << e.backtrace_str();
    } catch (std::exception &e) {
        LOG(FATAL) << e.what();
    }
}

void scheduler::attach_task(std::shared_ptr<task::pimpl> t) {
    ++_taskcount;
    t->_scheduler.reset(this);
    _alltasks.emplace_back(std::move(t));
}

void scheduler::remove_task(ptr<task::pimpl> t) {
    using namespace std;
    --_taskcount;
    DCHECK(t);
    DCHECK(t->_scheduler.get() == this);
    DCHECK(find(begin(_readyq), end(_readyq), t) == end(_readyq))
        << "BUG: " << t << " found in _readyq while being deleted";
    auto i = find_if(begin(_alltasks), end(_alltasks), [=](shared_ptr<task::pimpl> &tt) -> bool {
        return tt.get() == t.get();
    });
    DCHECK(i != end(_alltasks));
    _gctasks.emplace_back(*i);
    _alltasks.erase(i);
}



void scheduler::ready(ptr<task::pimpl> t, bool front) {
    DVLOG(5) << "readying: " << t;
    if (t->is_ready.exchange(true) == false) {
        if (this != &this_ctx->scheduler) {
            _dirtyq.push(t);
            wakeup();
        } else {
            if (front) {
                _readyq.push_front(t);
            } else {
                _readyq.push_back(t);
            }
        }
    }
}

void scheduler::ready_for_io(ptr<task::pimpl> t) {
    DVLOG(5) << "readying for io: " << t;
    if (t->is_ready.exchange(true) == false) {
        _readyq.push_back(t);
    }
}

void scheduler::unsafe_ready(ptr<task::pimpl> t) {
    DVLOG(5) << "readying: " << t;
    _readyq.push_back(t);
}

bool scheduler::cancel_task_by_id(uint64_t id) {
    bool found = false;
    for (auto &t : _alltasks) {
        if (t->id == id) {
            found = true;
            t->cancel();
            break;
        }
    }
    return found;
}

void scheduler::wakeup() {
    // TODO: speed this up?
    std::unique_lock<std::mutex> lock{_mutex};
    if (_io) {
        _io->wakeup();
    } else {
        _cv.notify_one();
    }
}

void scheduler::dump() const {
    LOG(INFO) << "scheduler: " << this;
    LOG(INFO) << ptr<task::pimpl>{_main_task.get()};
#ifdef TEN_TASK_TRACE
    LOG(INFO) << _main_task->_trace.str();
#endif
    for (auto &t : _alltasks) {
        LOG(INFO) << ptr<task::pimpl>{t.get()};
#ifdef TEN_TASK_TRACE
        LOG(INFO) << t->_trace.str();
#endif
    }
    FlushLogFiles(INFO);
}



} // ten

