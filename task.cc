#include "task.hh"
#include "runner.hh"

// static
atomic_count task::ntasks(0);

static void milliseconds_to_timespec(unsigned int ms, timespec &ts) {
    // convert milliseconds to seconds and nanoseconds
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
}

task::task(const proc &f_, size_t stack_size)
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
    if (to->state == state_idle)
        to->state = state_running;
    if (from->state == state_running)
        from->state = state_idle;
    // do the actual coro swap
    from->co.swap(&to->co);
    if (from->state == state_idle)
        from->state = state_running;
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
    task::self()->co.swap(&runner::self()->scheduler.co);
}

void task::start(task *t) {
    try {
        t->f();
    } catch(std::exception &e) {
        fprintf(stderr, "exception in task(%p): %s\n", t, e.what());
        abort();
    }
    // NOTE: the scheduler deletes tasks in exiting state
    // so this function won't ever return. don't expect
    // objects on this stack to have the destructor called
    t->state = state_exiting;
    t->co.swap(&runner::self()->scheduler.co);
}

void task::migrate(runner *to) {
    task *t = task::self();
    assert(t->co.main() == false);
    // if "to" is NULL, first available runner is used
    // or a new one is spawned
    // logic is in schedule()
    t->in = to;
    t->state = state_migrating;
    task::yield();
    // will resume in other runner
}

void task::sleep(unsigned int ms) {
    task *t = task::self();
    assert(t->co.main() == false);
    runner *r = runner::self();
    milliseconds_to_timespec(ms, t->ts);
    r->add_waiter(t);
    task::yield();
}

void task::suspend(mutex::scoped_lock &l) {
    assert(co.main() == false);
    state = task::state_idle;
    in = runner::self();
    l.unlock();
    task::yield();
    l.lock();
}

void task::resume() {
    assert(in->add_to_runqueue(this) == true);
}

bool task::poll(int fd, short events, unsigned int ms) {
    pollfd fds = {fd, events, 0};
    return task::poll(&fds, 1, ms);
}

int task::poll(pollfd *fds, nfds_t nfds, int timeout) {
    task *t = task::self();
    assert(t->co.main() == false);
    runner *r = runner::self();
    // TODO: maybe make a state for waiting io?
    t->state = state_idle;
    if (timeout) {
        milliseconds_to_timespec(timeout, t->ts);
    } else {
        t->ts.tv_sec = -1;
        t->ts.tv_nsec = -1;
    }
    r->add_waiter(t);
    r->add_pollfds(t, fds, nfds);

    // will be woken back up by epoll loop in runner::schedule()
    task::yield();

    return r->remove_pollfds(fds, nfds);
}

