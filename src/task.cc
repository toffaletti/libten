#include "ten/task/runtime.hh"
#include "ten/task/task_pimpl.hh"
#include "thread_context.hh"
#include <algorithm>
#include <ostream>

namespace ten {

std::ostream &operator << (std::ostream &o, const task_pimpl *t) {
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

/////// task_pimpl ///////

namespace {
    std::atomic<uint64_t> task_id_counter{0};
}

task_pimpl::cancellation_point::cancellation_point() {
    task_pimpl *t = runtime::current_task();
    ++t->_cancel_points;
}

task_pimpl::cancellation_point::~cancellation_point() {
    task_pimpl *t = runtime::current_task();
    --t->_cancel_points;
}

uint64_t task_pimpl::next_id() {
    return ++task_id_counter;
}

task_pimpl::task_pimpl()
    : _ctx(),
    _id{next_id()},
    _ready{false},
    _canceled{false}
{}

task_pimpl::task_pimpl(std::function<void ()> f)
    : _ctx{task_pimpl::trampoline},
    _id{next_id()},
    _f{std::move(f)},
    _ready{true},
    _canceled{false}
{}


void task_pimpl::trampoline(intptr_t arg) {
    task_pimpl *self = reinterpret_cast<task_pimpl *>(arg);
    try {
        if (!self->_canceled) {
            self->_f();
        }
    } catch (task_interrupted &e) {
    }
    self->_f = nullptr;

    thread_context *r = self->_runtime;
    r->remove_task(self);
    r->schedule();
    // never get here
    LOG(FATAL) << "Oh no! You fell through the trampoline " << self;
}

void task_pimpl::cancel() {
    if (_canceled.exchange(true) == false) {
        DVLOG(5) << "canceling: " << this << "\n";
        _runtime->ready(this);
    }
}

void task_pimpl::yield() {
    DVLOG(5) << "readyq yield " << this;
    task_pimpl::cancellation_point cancelable;
    if (_ready.exchange(true) == false) {
        _runtime->_readyq.push_back(this);
    }
    swap();
}

void task_pimpl::swap(bool nothrow) {
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

void task_pimpl::ready() {
    _runtime->ready(this);
}


///////////////////// task //////////////////

task::task(std::function<void ()> f)
    : _pimpl{std::make_shared<task_pimpl>(f)}
{
    runtime::attach(_pimpl);
}

task::task(task &&other) {
    if (this != &other) {
        std::swap(_pimpl, other._pimpl);
    }
}

task &task::operator=(task &&other) noexcept {
    if (this != &other) {
        std::swap(_pimpl, other._pimpl);
    }
    return *this;
}

void task::detach() {
    // TODO: implement
    throw errorx("unimplemented");
}

void task::join() {
    // TODO: implement
    throw errorx("unimplemented");
}

uint64_t task::get_id() const {
    return _pimpl->get_id();
}

void task::cancel() {
    _pimpl->cancel();
}


} // ten

