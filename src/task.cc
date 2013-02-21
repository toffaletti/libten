#include <cassert>
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
    DCHECK(this_ctx->scheduler.current_task());
    return this_ctx->scheduler.current_task()->id;
}

void yield() {
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    t->yield(); 
}

void sleep_until(const proc_time_t& sleep_time) {
    task::pimpl::cancellation_point cancellable;
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    scheduler::alarm_clock::scoped_alarm sleep_alarm{
        this_ctx->scheduler._alarms,
            t, sleep_time};
    t->swap();
}
} // this_task

void tasksleep(uint64_t ms) {
    this_task::sleep_for(std::chrono::milliseconds{ms});
}

bool fdwait(int fd, int rw, optional_timeout ms) {
    task::pimpl::cancellation_point cancellable;
    return this_ctx->scheduler.get_io().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
    task::pimpl::cancellation_point cancellable;
    return this_ctx->scheduler.get_io().poll(fds, nfds, ms);
}

uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    size_t ss = current_stacksize;
    if (ss != stacksize) {
        current_stacksize = stacksize;
    }
    task t = task::spawn(f);
    current_stacksize = ss;
    return t.get_id();
}

uint64_t taskid() {
    return this_task::get_id();
}

void taskyield() {
    this_task::yield();
}

bool taskcancel(uint64_t id) {
    return this_ctx->scheduler.cancel_task_by_id(id);
}

const char *taskname(const char *fmt, ...)
{
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetname(fmt, arg);
        va_end(arg);
    }
    return t->getname();
}

const char *taskstate(const char *fmt, ...)
{
	ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    if (fmt && strlen(fmt)) {
        va_list arg;
        va_start(arg, fmt);
        t->vsetstate(fmt, arg);
        va_end(arg);
    }
    return t->getstate();
}

void taskdump() {
    thread_context::dump_all();
}

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

struct deadline_pimpl {
    scheduler::alarm_clock::scoped_alarm alarm;
};

deadline::deadline(optional_timeout timeout) {
    if (timeout) {
        if (timeout->count() == 0)
            throw errorx("zero optional_deadline - misuse of optional");
        _set_deadline(*timeout);
    }
}

deadline::deadline(milliseconds ms) {
    _set_deadline(ms);
}

void deadline::_set_deadline(milliseconds ms) {
    if (ms.count() < 0)
        throw errorx("negative deadline: %jdms", intmax_t(ms.count()));
    if (ms.count() > 0) {
        ptr<task::pimpl> t = this_ctx->scheduler.current_task();
        auto now = this_ctx->scheduler.cached_time();
        _pimpl.reset(new deadline_pimpl{});
        _pimpl->alarm = std::move(scheduler::alarm_clock::scoped_alarm{
                this_ctx->scheduler._alarms, t, ms+now, deadline_reached{}
                });
        DVLOG(5) << "deadline alarm armed: " << _pimpl->alarm._armed << " in " << ms.count() << "ms";
    }
}

void deadline::cancel() {
    if (_pimpl) {
        _pimpl->alarm.cancel();
    }
}

deadline::~deadline() {
    cancel();
}

milliseconds deadline::remaining() const {
    if (_pimpl) {
        return _pimpl->alarm.remaining();
    }
    return {};
}

task::pimpl::cancellation_point::cancellation_point() {
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    ++t->cancel_points;
}

task::pimpl::cancellation_point::~cancellation_point() {
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    --t->cancel_points;
}


} // end namespace ten
