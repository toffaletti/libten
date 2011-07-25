#include "task.hh"
#include "runner.hh"

// static
atomic_count task::ntasks(0);

static void milliseconds_to_timespec(unsigned int ms, timespec &ts) {
    // convert milliseconds to seconds and nanoseconds
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
}

task::impl::impl(const proc &f_, size_t stack_size)
    : co((coroutine::proc)task::start, this, stack_size),
    f(f_), state(state_idle)
{
    ++ntasks;
}

task *task::self() {
    return runner::self()->get_task();
}

void task::swap(task *from, task *to) {
    // TODO: wrong place for this code. put in scheduler
    runner::self()->set_task(to);
    if (to->m->state == state_idle)
        to->m->state = state_running;
    if (from->m->state == state_running)
        from->m->state = state_idle;
    // do the actual coro swap
    from->m->co.swap(&to->m->co);
    if (from->m->state == state_idle)
        from->m->state = state_running;
    runner::self()->set_task(from);
    // don't modify to state after
    // because it might have been changed
}

task *task::spawn(const proc &f, runner *in) {
    task *t = new task(f);
    if (in) {
        in->add_to_runqueue(t);
    } else {
        runner::self()->add_to_runqueue(t);
    }
    return t;
}

void task::yield() {
    assert(task::self() != &runner::self()->scheduler);
    task::self()->m->co.swap(&runner::self()->scheduler.m->co);
}

void task::start(impl *i) {
    task t = i->to_task();
    try {
        t.m->f();
    } catch(std::exception &e) {
        fprintf(stderr, "exception in task(%p): %s\n", t.m.get(), e.what());
        abort();
    }
    // NOTE: the scheduler deletes tasks in exiting state
    // so this function won't ever return. don't expect
    // objects on this stack to have the destructor called
    t.m->state = state_exiting;
    t.m.reset();
    i->co.swap(&runner::self()->scheduler.m->co);
}

void task::migrate(runner *to) {
    task *t = task::self();
    assert(t->m->co.main() == false);
    // if "to" is NULL, first available runner is used
    // or a new one is spawned
    // logic is in schedule()
    t->m->in = to;
    t->m->state = state_migrating;
    task::yield();
    // will resume in other runner
}

void task::sleep(unsigned int ms) {
    task *t = task::self();
    assert(t->m->co.main() == false);
    runner *r = runner::self();
    milliseconds_to_timespec(ms, t->m->ts);
    r->add_waiter(t);
    task::yield();
}

void task::suspend(mutex::scoped_lock &l) {
    assert(m->co.main() == false);
    m->state = task::state_idle;
    m->in = runner::self();
    l.unlock();
    task::yield();
    l.lock();
}

void task::resume() {
    assert(m->in->add_to_runqueue(this) == true);
}

bool task::poll(int fd, short events, unsigned int ms) {
    pollfd fds = {fd, events, 0};
    return task::poll(&fds, 1, ms);
}

int task::poll(pollfd *fds, nfds_t nfds, int timeout) {
    task *t = task::self();
    assert(t->m->co.main() == false);
    runner *r = runner::self();
    // TODO: maybe make a state for waiting io?
    t->m->state = state_idle;
    if (timeout) {
        milliseconds_to_timespec(timeout, t->m->ts);
    } else {
        t->m->ts.tv_sec = -1;
        t->m->ts.tv_nsec = -1;
    }
    r->add_waiter(t);
    r->add_pollfds(t, fds, nfds);

    // will be woken back up by epoll loop in runner::schedule()
    task::yield();

    return r->remove_pollfds(fds, nfds);
}

