#include "thread_context.hh"
#include <inttypes.h>

namespace ten {

namespace {
static __thread size_t current_stacksize = SIGSTKSZ;
static std::atomic<uint64_t> taskidgen(0);
}

void task::set_default_stacksize(size_t stacksize) {
    CHECK(stacksize >= MINSIGSTKSZ);
    current_stacksize = stacksize;
}

std::ostream &operator << (std::ostream &o, ptr<task::impl> t) {
    if (t) {
        o << "[" << (void*)t.get() << " " << t->id << " "
          << t->getname() << " |" << t->getstate()
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
    const auto t = kernel::current_task();
    t->yield(); 
}

void sleep_until(const proc_time_t& sleep_time) {
    task::impl::cancellation_point cancellable;
    const auto t = kernel::current_task();
    scheduler::alarm_clock::scoped_alarm sleep_alarm{
        this_ctx->scheduler.arm_alarm(t, sleep_time)};
    t->swap();
}
} // this_task


task::task(const std::function<void ()> &f)
    : _impl{std::make_shared<task::impl>(f, current_stacksize)}
{
    this_ctx->scheduler.attach_task(_impl);
    // add new tasks to front of runqueue
    _impl->ready(true);
}

void task::entry(const std::function<void ()> &f) {
    try {
        f();
    } catch (task_interrupted &e) {
        DVLOG(5) << "task " << this_task::get_id() << " interrupted ";
    } catch (backtrace_exception &e) {
        LOG(ERROR) << "unhandled error: " << e.what() << "\n" << e.backtrace_str();
    } catch (std::exception &e) {
        LOG(ERROR) << "unhandled error: " << e.what();
    }
}

uint64_t task::get_id() const {
    return _impl->id;
}

task::impl::impl()
    : _ctx{},
    cancel_points{0},
    aux{new auxinfo{}},
    id{++taskidgen},
    fn{},
    is_ready{false},
    canceled{false}
{
    setname("main[%" PRId64 "]", id);
    setstate("new");
}

task::impl::impl(const std::function<void ()> &f, size_t stacksize)
    : _ctx{task::impl::trampoline, stacksize},
    cancel_points{0},
    aux{new auxinfo{}},
    id{++taskidgen},
    fn{f},
    is_ready{false},
    canceled{false}
{
    setname("task[%" PRId64 "]", id);
    setstate("new");
}

void task::impl::trampoline(intptr_t arg) {
    const ptr<task::impl> t{reinterpret_cast<task::impl *>(arg)};
    if (!t->canceled) {
        task::entry(t->fn);
    }
    t->fn = nullptr;
    const auto sched = t->_scheduler;
    sched->remove_task(t);
    sched->schedule();
    // never get here
    LOG(FATAL) << "Oh no! You fell through the trampoline in " << t;
}

void task::impl::setname(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetname(fmt, arg);
    va_end(arg);
}

void task::impl::vsetname(const char *fmt, va_list arg) {
    vsnprintf(aux->name, namesize, fmt, arg);
}

void task::impl::setstate(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vsetstate(fmt, arg);
    va_end(arg);
}

void task::impl::vsetstate(const char *fmt, va_list arg) {
    vsnprintf(aux->state, statesize, fmt, arg);
}

void task::impl::yield() {
    task::impl::cancellation_point cancellable;
    if (is_ready.exchange(true) == false) {
        setstate("yield");
        _scheduler->unsafe_ready(ptr<task::impl>{this});
    }
    swap();
}

void task::impl::safe_swap() noexcept {
    _scheduler->schedule();
}

void task::impl::swap() {
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

bool task::impl::cancelable() const {
    return cancel_points > 0;
}

void task::impl::cancel() {
    canceled = true;
    if (is_ready.exchange(true) == false) {
        this_ctx->scheduler.unsafe_ready(ptr<task::impl>{this});
    }
}

void task::impl::ready(bool front) {
    _scheduler->ready(ptr<task::impl>{this}, front);
}


void task::impl::ready_for_io() {
    _scheduler->ready_for_io(ptr<task::impl>{this});
}

task::impl::cancellation_point::cancellation_point() {
    const auto t = kernel::current_task();
    ++t->cancel_points;
}

task::impl::cancellation_point::~cancellation_point() {
    const auto t = kernel::current_task();
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
