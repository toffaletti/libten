#include <algorithm>
#include "thread_context.hh"

namespace ten {

// TODO: move to context.cc
thread_cached<stack_allocator::stack_cache, std::list<stack_allocator::stack>> stack_allocator::cache;

namespace {
static __thread size_t current_stacksize = SIGSTKSZ;
static std::atomic<uint64_t> taskidgen(0);
}

void task::set_default_stacksize(size_t stacksize) {
    CHECK(stacksize >= MINSIGSTKSZ);
    current_stacksize = stacksize;
}

std::ostream &operator << (std::ostream &o, ptr<task::pimpl> t) {
    if (t) {
        o << "[" << (void*)t.get() << " " << t->id << " "
            << t->name.get() << " |" << t->state.get()
            << "| canceled: " << t->canceled
            << " ready: " << t->is_ready << "]";
    } else {
        o << "nulltask";
    }
    return o;
}

namespace this_task {
uint64_t get_id() {
    DCHECK(kernel::current_task());
    return kernel::current_task()->id;
}

void yield() {
    ptr<task::pimpl> t = kernel::current_task();
    t->yield(); 
}

void sleep_until(const proc_time_t& sleep_time) {
    task::pimpl::cancellation_point cancellable;
    ptr<task::pimpl> t = kernel::current_task();
    scheduler::alarm_clock::scoped_alarm sleep_alarm{
        this_ctx->scheduler.arm_alarm(t, sleep_time)};
    t->swap();
}
} // this_task

task::task(const std::function<void ()> &f)
    : _pimpl{std::make_shared<task::pimpl>(f, current_stacksize)}
{
    this_ctx->scheduler.attach_task(_pimpl);
    // add new tasks to front of runqueue
    _pimpl->ready(true);
}

uint64_t task::get_id() const {
    return _pimpl->id;
}

task::pimpl::pimpl()
    : _ctx{},
    cancel_points{0},
    name{new char[namesize]},
    state{new char[statesize]},
    id{++taskidgen},
    fn{},
    is_ready{false},
    canceled{false}
{
    setname("main[%ju]", id);
    setstate("new");
}

task::pimpl::pimpl(const std::function<void ()> &f, size_t stacksize)
    : _ctx{task::pimpl::trampoline, stacksize},
    cancel_points{0},
    name{new char[namesize]},
    state{new char[statesize]},
    id{++taskidgen},
    fn{f},
    is_ready{false},
    canceled{false}
{
    setname("task[%ju]", id);
    setstate("new");
}

void task::pimpl::trampoline(intptr_t arg) {
    ptr<task::pimpl> self{reinterpret_cast<task::pimpl *>(arg)};
    try {
        if (!self->canceled) {
            self->fn();
        }
    } catch (task_interrupted &e) {
        DVLOG(5) << self << " interrupted ";
    } catch (backtrace_exception &e) {
        LOG(ERROR) << "unhandled error in " << self << ": " << e.what() << "\n" << e.backtrace_str();
    } catch (std::exception &e) {
        LOG(ERROR) << "unhandled error in " << self << ": " << e.what();
    }
    self->fn = nullptr;
    ptr<scheduler> sched = self->_scheduler;
    sched->remove_task(self);
    self->swap();
    // never get here
    LOG(FATAL) << "Oh no! You fell through the trampoline " << self;
}

void task::pimpl::setname(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetname(fmt, arg);
    va_end(arg);
}

void task::pimpl::vsetname(const char *fmt, va_list arg) {
    vsnprintf(name.get(), namesize, fmt, arg);
}

void task::pimpl::setstate(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetstate(fmt, arg);
    va_end(arg);
}

void task::pimpl::vsetstate(const char *fmt, va_list arg) {
    vsnprintf(state.get(), statesize, fmt, arg);
}

void task::pimpl::yield() {
    task::pimpl::cancellation_point cancellable;
    if (is_ready.exchange(true) == false) {
        setstate("yield");
        _scheduler->unsafe_ready(ptr<task::pimpl>{this});
    }
    swap();
}

void task::pimpl::safe_swap() noexcept {
    _scheduler->schedule();
}

void task::pimpl::swap() {
    _scheduler->schedule();
    //ctx.swap(this_ctx->scheduler.sched_context(), 0);

    if (canceled && cancel_points > 0) {
        DVLOG(5) << "THROW INTERRUPT: " << this << "\n" << saved_backtrace().str();
        throw task_interrupted();
    }

    if (exception && cancel_points > 0) {
        DVLOG(5) << "THROW TIMEOUT: " << this << "\n" << saved_backtrace().str();
        std::exception_ptr tmp = nullptr;
        std::swap(tmp, exception);
        std::rethrow_exception(tmp);
    }
}

bool task::pimpl::cancelable() const {
    return cancel_points > 0;
}

void task::pimpl::cancel() {
    canceled = true;
    if (is_ready.exchange(true) == false) {
        this_ctx->scheduler.unsafe_ready(ptr<task::pimpl>{this});
    }
}

void task::pimpl::ready(bool front) {
    _scheduler->ready(ptr<task::pimpl>{this}, front);
}


void task::pimpl::ready_for_io() {
    _scheduler->ready_for_io(ptr<task::pimpl>{this});
}

task::pimpl::cancellation_point::cancellation_point() {
    ptr<task::pimpl> t = kernel::current_task();
    ++t->cancel_points;
}

task::pimpl::cancellation_point::~cancellation_point() {
    ptr<task::pimpl> t = kernel::current_task();
    --t->cancel_points;
}

// should be in compat.cc but here because of current_stacksize mess
uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    size_t ss = current_stacksize;
    if (ss != stacksize) {
        current_stacksize = stacksize;
    }
    task t = task::spawn(f);
    current_stacksize = ss;
    return t.get_id();
}



} // end namespace ten
