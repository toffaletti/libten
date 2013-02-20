#include <cassert>
#include <algorithm>
#include "proc.hh"
#include "io.hh"

namespace ten {

namespace {
static __thread size_t current_stacksize = SIGSTKSZ;
}


void task::set_default_stacksize(size_t stacksize) {
    CHECK(stacksize >= MINSIGSTKSZ);
    current_stacksize = stacksize;
}

// TODO: move to context.cc
thread_cached<stack_allocator::stack_cache, std::list<stack_allocator::stack>> stack_allocator::cache;

static std::atomic<uint64_t> taskidgen(0);

std::ostream &operator << (std::ostream &o, ptr<task::pimpl> t) {
    if (t) {
        o << "[" << (void*)t.get() << " " << t->id << " "
            << t->name.get() << " |" << t->state.get()
            << "| sys: " << t->systask
            << " canceled: " << t->canceled << "]";
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
    task::pimpl::cancellation_point cancellable;
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    if (t->is_ready.exchange(true) == false) {
        t->setstate("yield");
        this_ctx->scheduler.unsafe_ready(t);
    }
    t->swap();
}

void sleep_until(const proc_time_t& sleep_time) {
    task::pimpl::cancellation_point cancellable;
    ptr<task::pimpl> t = this_ctx->scheduler.current_task();
    io_scheduler::alarm_clock::scoped_alarm sleep_alarm{
        this_ctx->scheduler.sched().alarms,
            t, sleep_time};
    t->swap();
}

} // this_task

void tasksleep(uint64_t ms) {
    this_task::sleep_for(std::chrono::milliseconds{ms});
}

bool fdwait(int fd, int rw, optional_timeout ms) {
    task::pimpl::cancellation_point cancellable;
    return this_ctx->scheduler.sched().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
    task::pimpl::cancellation_point cancellable;
    return this_ctx->scheduler.sched().poll(fds, nfds, ms);
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

void tasksystem() {
    this_ctx->scheduler.mark_system_task();
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
    return t->name.get();
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
    return t->state.get();
}

std::string taskdump() {
    std::stringstream ss;
    this_ctx->scheduler.dump_tasks(ss);
    return ss.str();
}

void taskdumpf(FILE *of) {
    std::string dump = taskdump();
    fwrite(dump.c_str(), sizeof(char), dump.size(), of);
    fflush(of);
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

task::pimpl::pimpl(const std::function<void ()> &f, size_t stacksize)
    : ctx{task::pimpl::start, stacksize},
    cancel_points{0},
    name{new char[namesize]},
    state{new char[statesize]},
    id{++taskidgen},
    fn{f},
    is_ready{false},
    systask{false},
    canceled{false}
{
    setname("task[%ju]", id);
    setstate("new");
}

void task::pimpl::start(intptr_t arg) {
    task::pimpl *t = reinterpret_cast<task::pimpl *>(arg);
    try {
        if (!t->canceled) {
            t->fn();
        }
    } catch (task_interrupted &e) {
        DVLOG(5) << t << " interrupted ";
    } catch (backtrace_exception &e) {
        LOG(ERROR) << "unhandled error in " << t << ": " << e.what() << "\n" << e.backtrace_str();
        std::exit(2);
    } catch (std::exception &e) {
        LOG(ERROR) << "unhandled error in " << t << ": " << e.what();
        std::exit(2);
    }
    t->exit();
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

void task::pimpl::safe_swap() noexcept {
    // swap to scheduler coroutine
    ctx.swap(this_ctx->scheduler.sched_context(), 0);
}

void task::pimpl::swap() {
    // swap to scheduler coroutine
    ctx.swap(this_ctx->scheduler.sched_context(), 0);

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

void task::pimpl::exit() {
    fn = nullptr;
    swap();
}

bool task::pimpl::cancelable() const {
    return cancel_points > 0;
}

void task::pimpl::cancel() {
    // don't cancel systasks
    if (systask) return;
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
    io_scheduler::alarm_clock::scoped_alarm alarm;
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
        _pimpl->alarm = std::move(io_scheduler::alarm_clock::scoped_alarm{
                this_ctx->scheduler.sched().alarms, t, ms+now, deadline_reached{}
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
