#include "ten/task2/task.hh"
#include <atomic>
#include <array>
#include <algorithm>

namespace ten {
namespace task2 {

std::ostream &operator << (std::ostream &o, const task *t) {
    o << "task[" << (void *)t << "]";
    return o;
}

std::ostream &operator << (std::ostream &o, task::state s) {
    static std::array<const char *, 6> names = {{
        "fresh",
        "ready",
        "asleep",
        "canceled",
        "unwinding",
        "finished"
    }};
    o << names[static_cast<int>(s)];
    return o;
}

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

template <class Arg>
bool valid_states(Arg to) {
    return false;
}

template <class Arg, class... Args>
bool valid_states(Arg to, Arg valid, Args ...others) {
    if (to == valid) return true;
    return valid_states(to, others...);
}

bool task::transition(state to) {
    bool valid = true;
    while (valid) {
        state from = _state;
        // always ok to transition to the state we're already in
        if (from == to) return true;
        switch (from) {
            case state::fresh:
                // from fresh we can go directly to finished
                // without needing to unwind
                if (to == state::canceled) {
                    to = state::finished;
                }
                valid = valid_states(to, state::ready, state::finished);
                break;
            case state::ready:
                valid = valid_states(to, state::asleep, state::canceled, state::finished);
                break;
            case state::asleep:
                valid = valid_states(to, state::ready, state::canceled);
                break;
            case state::canceled:
                valid = valid_states(to, state::unwinding, state::finished);
                break;
            case state::unwinding:
                valid = valid_states(to, state::finished);
                break;
            case state::finished:
                valid = valid_states(to);
                break;
            default:
                // bug
                std::terminate();
        }

        if (valid) {
            if (_state.compare_exchange_weak(from, to)) {
                return true;
            }
        }
    }
    return false;
}

void task::cancel() {
    if (transition(state::canceled)) {
        DVLOG(5) << "canceling: " << this << "\n";
        _runtime->ready(this);
    }
}

void task::join() {
    coroutine::join();
}

void task::yield() {
    this_coro::yield();

    state st = _state;
    if (st == state::canceled && _cancel_points > 0) {
        if (transition(state::unwinding)) {
            DVLOG(5) << "unwinding task: " << this;
            throw coroutine_cancel();
        }
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

#if 0
void task::ready() {
    state st = _state;
    // TODO: clean this up to use transition
    if (st == state::canceled) {
        if (transition(state::unwinding)) {
            _runtime->ready(this);
        }
    } else if (st != state::ready) {
        if (transition(state::ready)) {
            _runtime->ready(this);
        }
    }
}
#endif

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
                if (t->transition(task::state::ready)) {
                    ready(t);
                } else {
                    DVLOG(5) << "wtf?";
                }
            } else  {
                break;
            }
        }
        _timeout_tasks.erase(std::begin(_timeout_tasks), i);

        if (!_readyq.empty()) {
            auto t = _readyq.front();
            _readyq.pop_front();
            if (t->_state == task::state::fresh) {
                t->transition(task::state::ready);
            }
            if (t->runnable()) {
                DVLOG(5) << "task: " << t << " state: " << t->_state.load();
                if (_task.yield_to(*t)) {
                    DVLOG(5) << "post yield task: " << t << " state: " << t->_state.load();
                    // TODO: this is ugly.
                    // yield_to return value and done() should be unified
                    // need to re-think task and coroutine.
                    // perhaps should have a basic context and task
                    // coroutine and task are already incompat
                    // it also feels like state needs to be integrated into
                    // coroutine. so maybe task+coroutine become one thing
                    // and re-introduce context?
                    if (t->done()) {
                        DVLOG(5) << "task finished: " << t;
                        t->transition(task::state::finished);
                    } else if (t->runnable()) {
                        _readyq.push_back(t);
                    }
                } else {
                    DVLOG(5) << "task finished: " << t;
                    t->transition(task::state::finished);
                }
            }
            if (t->_state == task::state::finished) {
                DVLOG(5) << "task finished: " << t;
                _timeout_tasks.remove(t);
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

