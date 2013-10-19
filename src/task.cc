#include "thread_context.hh"
#include <stdexcept>
#include <inttypes.h>

namespace ten {

namespace {
std::atomic<uint64_t> taskidgen(0);
}

std::ostream &operator << (std::ostream &o, ptr<task::impl> t) {
    if (t) {
        o << "[" << (void*)t.get() << " " << t->get_id() << " "
          << t->getname() << " |" << t->getstate()
          << "| canceled: " << t->_canceled
          << " ready: " << t->_ready << "]";
    } else {
        o << "nulltask";
    }
    return o;
}

namespace this_task {
uint64_t get_id() {
    DCHECK(scheduler::current_task());
    return scheduler::current_task()->get_id();
}

void yield() {
    const auto t = scheduler::current_task();
    t->yield();
}

void sleep_until(const kernel::time_point& sleep_time) {
    task::impl::cancellation_point cancellable;
    const auto t = scheduler::current_task();
    scheduler::alarm_clock::scoped_alarm sleep_alarm{
        this_ctx->scheduler.arm_alarm(t, sleep_time)};
    t->swap();
}
} // this_task


task::task(std::function<void ()> f)
    : _impl{std::make_shared<task::impl>(std::move(f), stack_allocator::default_stacksize)}
{
    this_ctx->scheduler.attach_task(_impl);
    // add new tasks to front of runqueue
    _impl->ready(true);
}

uint64_t task::get_id() const {
    if (_impl) {
        return _impl->get_id();
    }
    return 0;
}

void task::cancel() {
    if (_impl) {
        _impl->cancel();
    }
}

bool task::joinable() noexcept {
    return (bool)_impl;
}

void task::join() {
    if (_impl) {
        _impl->join();
    }
    else {
        throw std::system_error{EPERM, std::system_category(), "task not joinable"};
    }
}

task::impl::impl()
    : _ctx{},
    _cancel_points{0},
    _aux{new auxinfo{}},
    _id{++taskidgen},
    _fn{},
    _ready{false},
    _canceled{false}
{
    setname("main[%" PRId64 "]", _id);
    setstate("new");
}

task::impl::impl(std::function<void ()> f, size_t stacksize)
    : _ctx{task::impl::trampoline, stacksize},
    _cancel_points{0},
    _aux{new auxinfo{}},
    _id{++taskidgen},
    _fn{std::move(f)},
    _ready{false},
    _canceled{false}
{
    setname("task[%" PRId64 "]", _id);
    setstate("new");
}

void task::impl::trampoline(intptr_t arg) {
    const ptr<task::impl> t{reinterpret_cast<task::impl *>(arg)};
    if (!t->_canceled) {
        task::entry(t->_fn);
    }
    t->_fn = nullptr;
    t->_join([](joininfo &i){
        DCHECK(!i.finished);
        i.finished = true;
        if (i.joiner) {
            i.joiner->ready();
            i.joiner = nullptr;
        }
    });
    const auto sched = t->_scheduler;
    sched->remove_task(t);
    sched->schedule();
    // never get here
    LOG(FATAL) << "Oh no! You fell through the trampoline in " << t;
}

int task::introduce(std::function<void()> f) {
    thread_context ctx;
    return task::entry(f);
}

int task::entry(std::function<void()> f) {
    try {
        f();
        return EXIT_SUCCESS;
    } catch (deadline_reached &e) {
        DVLOG(5) << "task " << ten::this_task::get_id() << " crossed a deadline";
    } catch (task_interrupted &e) {
        DVLOG(5) << "task " << ten::this_task::get_id() << " interrupted";
    } catch (backtrace_exception &e) {
        LOG(ERROR) << "unhandled exception: " << e.what() << "\n" << e.backtrace_str();
    } catch (std::exception &e) {
        LOG(ERROR) << "unhandled exception: " << e.what();
    }
    return EXIT_FAILURE;
}

void task::impl::setname(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetname(fmt, arg);
    va_end(arg);
}

void task::impl::vsetname(const char *fmt, va_list arg) {
    vsnprintf(_aux->name, namesize, fmt, arg);
}

void task::impl::setstate(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetstate(fmt, arg);
    va_end(arg);
}

void task::impl::vsetstate(const char *fmt, va_list arg) {
    vsnprintf(_aux->state, statesize, fmt, arg);
}

void task::impl::yield() {
    task::impl::cancellation_point cancellable;
    if (_ready.exchange(true) == false) {
        setstate("yield");
        _scheduler->yield_ready(ptr<task::impl>{this});
    }
    swap();
}

void task::impl::safe_swap() noexcept {
    _scheduler->schedule();
}

void task::impl::swap() {
    _scheduler->schedule();
    //ctx.swap(this_ctx->scheduler.sched_context(), 0);

    if (_canceled && _cancel_points > 0) {
        DVLOG(5) << "THROW INTERRUPT: " << this << "\n" << saved_backtrace().str();
        throw task_interrupted();
    }

    if (_exception && _cancel_points > 0) {
        DVLOG(5) << "THROW TIMEOUT: " << this << "\n" << saved_backtrace().str();
        std::exception_ptr tmp;
        std::swap(tmp, _exception);
        std::rethrow_exception(tmp);
    }
}

bool task::impl::cancelable() const {
    return _cancel_points > 0;
}

void task::impl::cancel() {
    if (_canceled.exchange(true) == false) {
        DVLOG(5) << "canceling " << ptr<task::impl>{this};
        ready();
    }
}

void task::impl::join() noexcept {
    const auto t = scheduler::current_task();
    bool finished = _join([t](joininfo &i) {
        if (!i.finished) {
            CHECK(i.joiner == nullptr);
            i.joiner = t;
        }
        return i.finished;
    });
    if (!finished) {
        // join isn't interruptable
        t->safe_swap();
    }
    DCHECK(_join([](joininfo &i) { return i.finished; }));
}

void task::impl::ready(bool front) {
    _scheduler->ready(ptr<task::impl>{this}, front);
}


void task::impl::ready_for_io() {
    _scheduler->ready_for_io(ptr<task::impl>{this});
}

task::impl::cancellation_point::cancellation_point() {
    const auto t = scheduler::current_task();
    ++t->_cancel_points;
}

task::impl::cancellation_point::~cancellation_point() {
    const auto t = scheduler::current_task();
    --t->_cancel_points;
}

// should be in compat.cc but here because of current_stacksize mess
uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    // XXX: stacksize is ignored now
    task t = task::spawn(std::move(f));
    return t.get_id();
}

} // end namespace ten
