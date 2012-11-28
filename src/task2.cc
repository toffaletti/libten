#include "ten/task2/task.hh"
#include <atomic>
#include <algorithm>

namespace ten {
namespace task2 {

task *runtime::current_task() {
    return static_cast<task *>(this_coro::get());
}

namespace this_task {
uint64_t get_id() {
    return runtime::current_task()->get_id();
}

void yield() {
    runtime::current_task()->yield();
}
} // end namespace this_task

/////// task ///////

namespace {
    std::atomic<uint64_t> task_id_counter{0};
}

task::cancellation_point::cancellation_point() {
    task *t = runtime::current_task();
    ++t->_cancel_points;
}

task::cancellation_point::~cancellation_point() {
    task *t = runtime::current_task();
    --t->_cancel_points;
}

uint64_t task::next_id() {
    return ++task_id_counter;
}

void task::cancel() {
    state st = _state;
    if (st != state::canceling) {
        DVLOG(5) << "canceling: " << this << "\n";
        _state = state::canceled;
        ready();
    }
}

void task::join() {
    coroutine::join();
}

void task::yield() {
    this_coro::yield();

    state st = _state;
    if (st == state::canceling && _cancel_points > 0) {
        DVLOG(5) << "canceling task!\n";
        throw coroutine_cancel();
    }

    while (!_timeouts.empty()) {
        timeout *to = _timeouts.front();
        if (to->when <= runtime::now()) {
            _timeouts.pop_front();
            std::unique_ptr<timeout> tmp{to};
            //DVLOG(5) << to << " reached for " << this << " removing.";
            if (_timeouts.empty()) {
                // remove from scheduler timeout list
                runtime::_runtime->_timeout_tasks.remove(this);
            }
            if (tmp->exception != nullptr) {
                //DVLOG(5) << "THROW TIMEOUT: " << this << "\n" << saved_backtrace().str();
                std::rethrow_exception(tmp->exception);
            }
        } else {
            break;
        }
    }
}

void task::ready() {
    state st = _state;
    if (st == state::canceled) {
        if (_state.compare_exchange_weak(st, state::canceling)) {
            _runtime->ready(this);
        }
    } else if (st != state::ready) {
        if (_state.compare_exchange_weak(st, state::ready)) {
            _runtime->ready(this);
        }
    }
}

__thread runtime *runtime::_runtime = nullptr;

void runtime::ready(task *t) {
    if (this != _runtime) {
        _dirtyq.push(t);
        // TODO: speed this up?
        std::unique_lock<std::mutex> lock{_mutex};
        _cv.notify_one();
    } else {
        _readyq.push_back(t);
    }
}

void runtime::operator()() {
    while (!_alltasks.empty()) {
        {
            task *t = nullptr;
            while (_dirtyq.pop(t)) {
                _readyq.push_back(t);
            }
        }

        auto now = update_cached_time();
        auto i = std::begin(_timeout_tasks);
        for (; i != std::end(_timeout_tasks); ++i) {
            task *t = *i;
            if (t->first_timeout() <= now) {
                t->ready();
            } else  {
                break;
            }
        }
        _timeout_tasks.erase(std::begin(_timeout_tasks), i);

        if (!_readyq.empty()) {
            auto t = _readyq.front();
            _readyq.pop_front();
            //assert(t->_state == task::state::ready);
            if (_task.yield_to(*t)) {
                if (t->_state == task::state::ready) {
                    _readyq.push_back(t);
                }
            } else {
                auto i = std::find_if(std::begin(_alltasks), std::end(_alltasks),
                        [t](shared_task &other) {
                            return other.get() == t;
                        });
                _alltasks.erase(i);
            }
        } else {
            std::unique_lock<std::mutex> lock{_mutex};
            if (_timeout_tasks.empty()) {
                _cv.wait(lock);
            } else {
                _cv.wait_until(lock, _timeout_tasks.front()->_timeouts.front()->when);
            }
        }
    }
}

} // task2
} // ten

