#include "ten/task2/task.hh"
#include <atomic>
#include <algorithm>

namespace ten {
namespace task2 {

namespace this_task {
namespace {
    __thread task *current_task = nullptr;
} // end anon namespace

uint64_t get_id() {
    return current_task->get_id();
}

void yield() {
    current_task->yield();
}

} // end namespace this_task

/////// task ///////

namespace {
    std::atomic<uint64_t> task_id_counter{0};
}

uint64_t task::next_id() {
    return ++task_id_counter;
}

void task::join() {
    _coro.join();
}

void task::yield() {
    this_coro::yield();
}

__thread runtime *runtime::_runtime = nullptr;

void runtime::operator()() {
    while (!_alltasks.empty()) {
        while (!_readyq.empty()) {
            auto t = _readyq.front();
            _readyq.pop_front();
            if (_coro.yield_to(t->_coro)) {
                _readyq.push_back(t);
            } else {
                auto i = std::find_if(std::begin(_alltasks), std::end(_alltasks),
                        [t](shared_task &other) {
                            return other.get() == t;
                        });
                _alltasks.erase(i);
            }
        }
    }
}

} // task2
} // ten

