#include "ten/task/runtime.hh"
#include <algorithm>

namespace ten {

std::ostream &operator << (std::ostream &o, const task *t) {
    if (t) {
        o << "task[" << t->_id
            << "," << (void *)t
            << ",ready:" << t->_ready
            << ",canceled:" << t->_canceled
            << "]";
    } else {
        o << "task[null]";
    }
    return o;
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

task::task()
    : _ctx(),
    _id{next_id()},
    _ready{false},
    _canceled{false}
{}

task::task(std::function<void ()> f)
    : _ctx{task::trampoline},
    _id{next_id()},
    _f{std::move(f)},
    _ready{true},
    _canceled{false}
{}


void task::trampoline(intptr_t arg) {
    task *self = reinterpret_cast<task *>(arg);
    try {
        if (!self->_canceled) {
            self->_f();
        }
    } catch (task_interrupted &e) {
    }
    self->_f = nullptr;

    runtime *r = self->_runtime;
    r->remove_task(self);
    r->schedule();
    // never get here
    LOG(FATAL) << "Oh no! You fell through the trampoline " << self;
}

void task::cancel() {
    if (_canceled.exchange(true) == false) {
        DVLOG(5) << "canceling: " << this << "\n";
        _runtime->ready(this);
    }
}

void task::join() {
    // TODO: implement
}

void task::yield() {
    DVLOG(5) << "readyq yield " << this;
    task::cancellation_point cancelable;
    if (_ready.exchange(true) == false) {
        _runtime->_readyq.push_back(this);
    }
    swap();
}

void task::swap(bool nothrow) {
    _runtime->schedule();
    if (nothrow) return;

    if (_canceled && _cancel_points > 0) {
        if (!_unwinding) {
            _unwinding = true;
            DVLOG(5) << "unwinding task: " << this;
            throw task_interrupted();
        }
    }

    // TODO: safe_swap
    if (_exception != nullptr) {
        std::exception_ptr exception = _exception;
        _exception = nullptr;
        std::rethrow_exception(exception);
    }
}

void task::ready() {
    _runtime->ready(this);
}

} // ten

