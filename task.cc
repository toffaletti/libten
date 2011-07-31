#include "task.hh"
#include "runner.hh"
#include "channel.hh"

// static
atomic_count task::ntasks(0);

static void milliseconds_to_timespec(unsigned int ms, timespec &ts) {
    // convert milliseconds to seconds and nanoseconds
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
}

task::impl::impl(const proc &f_, size_t stack_size)
    : co((coroutine::proc)task::start, this, stack_size),
    f(f_), flags(_TASK_SLEEP)
{
    ++ntasks;
}

task task::self() {
    return runner::self().get_task();
}

void task::swap(task &from, task &to) {
    assert(!(to.m->flags & _TASK_RUNNING));
    to.m->flags |= _TASK_RUNNING;

    assert(from.m->flags & _TASK_RUNNING);
    from.m->flags ^= _TASK_RUNNING;

    from.m->co.swap(&to.m->co);

    assert(to.m->flags & _TASK_RUNNING);
    to.m->flags ^= _TASK_RUNNING;

    assert(!(from.m->flags & _TASK_RUNNING));
    from.m->flags |= _TASK_RUNNING;
}

task task::spawn(const proc &f, runner *in) {
    task t(f);
    if (in) {
        in->add_to_runqueue(t);
    } else {
        runner::self().add_to_runqueue(t);
    }
    return t;
}

void task::yield() {
    runner::swap_to_scheduler();
}

void task::start(impl *i) {
    task t = i->to_task();
    try {
        t.m->f();
    } catch (channel_closed_error &e) {
        fprintf(stderr, "caught channel close error in task(%p)\n", t.m.get());
    } catch(std::exception &e) {
        fprintf(stderr, "exception in task(%p): %s\n", t.m.get(), e.what());
        abort();
    }
    // NOTE: the scheduler deletes tasks in exiting state
    // so this function won't ever return. don't expect
    // objects on this stack to have the destructor called
    t.m->flags |= _TASK_EXIT;
    t.m.reset();
    // this is a dangerous bit of code, the shared_ptr is reset
    // potentially freeing the impl *, however the scheduler
    // should always be holding a reference, so it is "safe"

    runner::swap_to_scheduler();
}

void task::migrate(runner *to) {
    task t = task::self();
    assert(t.m->co.main() == false);
    // if "to" is NULL, first available runner is used
    // or a new one is spawned
    // logic is in schedule()
    if (to)
        t.m->in = *to;
    else
        t.m->in.m.reset();
    t.m->flags |= _TASK_MIGRATE;
    task::yield();
    // will resume in other runner
}

void task::sleep(unsigned int ms) {
    task t = task::self();
    assert(t.m->co.main() == false);
    milliseconds_to_timespec(ms, t.m->ts);
    runner::self().add_waiter(t);
    task::yield();
}

void task::suspend(mutex::scoped_lock &l) {
    assert(m->co.main() == false);
    m->flags |= _TASK_SLEEP;
    m->in = runner::self();
    l.unlock();
    task::yield();
    l.lock();
}

void task::resume() {
    task t = m->to_task();
    assert(m->in.add_to_runqueue(t) == true);
}

bool task::poll(int fd, short events, unsigned int ms) {
    pollfd fds = {fd, events, 0};
    return task::poll(&fds, 1, ms);
}

int task::poll(pollfd *fds, nfds_t nfds, int timeout) {
    task t = task::self();
    assert(t.m->co.main() == false);
    runner r = runner::self();
    // TODO: maybe make a state for waiting io?
    //t.m->flags |= _TASK_POLL;
    t.m->flags |= _TASK_SLEEP;
    if (timeout) {
        milliseconds_to_timespec(timeout, t.m->ts);
    } else {
        t.m->ts.tv_sec = -1;
        t.m->ts.tv_nsec = -1;
    }
    r.add_waiter(t);
    r.add_pollfds(t, fds, nfds);

    // will be woken back up by epoll loop in runner::schedule()
    task::yield();

    return r.remove_pollfds(fds, nfds);
}

void task::set_runner(runner &i) {
    m->in = i;
}

runner &task::get_runner() const {
    return m->in;
}

