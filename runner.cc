#include "runner.hh"
#include "task.hh"
#include <cxxabi.h>

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace hack {
// hack to avoid redefinition of ucontext
// by including it in a namespace
#include <asm-generic/ucontext.h>
}

#include <unistd.h>

#include <stdexcept>
#include <algorithm>
#include <set>

// TODO: replace std::cerr with better (thread safe) logging
#include <iostream>

__thread runner::impl *runner::impl_ = NULL;

unsigned int runner::thread_timeout_ms = 60*1000;
thread runner::main_thread_ = thread::self();
unsigned long runner::ncpu_ = 0;
runner::list *runner::runners = new runner::list;
mutex *runner::tmutex = new mutex;

// thrown when runner has no tasks for thread_timeout_ms
// runner will then be removed from runners list
struct runner_timeout_exit : std::exception {};

template <typename SetT> struct in_set {
    SetT &set;

    in_set(SetT &s) : set(s) {}

    bool operator()(const typename SetT::key_type &k) {
        return set.count(k) > 0;
    }
};

static std::ostream &operator << (std::ostream &o, task::deque &l) {
    o << "[";
    for (task::deque::iterator i=l.begin(); i!=l.end(); ++i) {
        o << *i << ",";
    }
    o << "]";
    return o;
}

//! internal data members
struct runner::impl : boost::noncopyable, boost::enable_shared_from_this<impl> {
    //! pthread this runner is running in
    thread tt;
    mutex mut;
    scheduler *s;

    //! constructor for main thread runner
    impl() : s(NULL) {
        s = scheduler::create();
        tt=thread::self();
    }

    //! constructor for runners that create a new thread
    impl(task &t) : s(NULL) {
        s = scheduler::create();
        add_to_runqueue(t);
        thread::create(tt, runner::start, this);
    }

    ~impl() {
        mutex::scoped_lock l(mut);
        //current_task.m.reset();
        delete s;
    }

    void wakeup() {
        mutex::scoped_lock l(mut);
        s->wakeup();
    }

    //! lock must already be held before calling this
    void wakeup_nolock() {
        s->wakeup();
    }

    //! add task to the end of the run queue
    void add_to_runqueue(task &t) {
        mutex::scoped_lock l(mut);
        s->add_to_runqueue(t);
    }

    // tmutex must be held, shared_from_this is not thread safe
    runner to_runner(mutex::scoped_lock &) { return shared_from_this(); }
};

runner::runner() {}
runner::runner(const shared_impl &m_) : m(m_) {}
runner::runner(task &t) : m(new impl(t)) {}

void runner::append_to_list(runner &r, mutex::scoped_lock &l) {
    runners->push_back(r);
}

void runner::remove_from_list(runner &r) {
    mutex::scoped_lock l(*tmutex);
    runners->remove(r);
}

void runner::wakeup_all_runners() {
    runner current = runner::self();
    mutex::scoped_lock l(*tmutex);
    for (runner::list::iterator i=runners->begin(); i!=runners->end(); ++i) {
        if (current == *i) continue;
        i->m->wakeup();
    }
}

unsigned long runner::ncpu() {
    if (ncpu_ == 0) {
        ncpu_ = sysconf(_SC_NPROCESSORS_ONLN);
    }
    return ncpu_;
}

runner runner::self() {
    mutex::scoped_lock l(*tmutex);
    return impl_->to_runner(l);
}

runner runner::spawn(task &t) {
    mutex::scoped_lock l(*tmutex);
    runner r(t);
    append_to_list(r, l);
    return r;
}

runner runner::spawn(const task::proc &f, bool force, size_t stack_size) {
    mutex::scoped_lock l(*tmutex);
    if (runners->size() >= ncpu() && !force) {
        // reuse an existing runner, round robin
        runner r = runners->front();
        runners->pop_front();
        runners->push_back(r);
        task::spawn(f, &r);
        return r;
    }
    task t(f, stack_size);
    runner r(t);
    append_to_list(r, l);
    return r;
}

runner::operator unspecified_bool_type() const {
    return m.get();
}

bool runner::operator == (const runner &r) const {
    return m.get() == r.m.get();
}

void runner::schedule() {
    // check use_count because use could be holding onto this
    // runner to add a task later. is there a use case for this?
    while (task::get_ntasks() > 0 || m.use_count() > 2) {
        mutex::scoped_lock l(m->mut);
        m->s->schedule(*this, l, thread_timeout_ms);
    }

    wakeup_all_runners();

    // spin waiting for other runners to exit
    // this is mostly to prevent valgrind warnings
    // about threads exiting while holding locks
    mutex::scoped_lock l(*tmutex);
    if (m->tt == main_thread_) {
        while (runners->size() > 1) {
            l.unlock();
            thread::yield();
            l.lock();
        }
    }
}

void runner::add_pollfds(task &t, pollfd *fds, nfds_t nfds) {
    m->s->add_pollfds(t, fds, nfds);
}

int runner::remove_pollfds(pollfd *fds, nfds_t nfds) {
    return m->s->remove_pollfds(fds, nfds);
}

void runner::add_to_runqueue(task &t) {
    m->add_to_runqueue(t);
}

void *runner::start(void *arg) {
    mutex::scoped_lock l(*tmutex);
    impl_ = ((runner::impl *)arg);
    thread::self().detach();
    runner r;
    r = impl_->to_runner(l);
    l.unlock();
    try {
        r.schedule();
    } catch (abi::__forced_unwind&) {
        remove_from_list(r);
        l.lock();
        impl_ = NULL;
        throw;
    } catch (runner_timeout_exit &e) {
        // more runners than cpus and nothing to do
    } catch (backtrace_exception &e) {
        fprintf(stderr, "uncaught exception in runner: %s\n%s\n", e.what(), e.str().c_str());
        remove_from_list(r);
        l.lock();
        impl_ = NULL;
        throw;
    } catch (std::exception &e) {
        fprintf(stderr, "uncaught exception in runner: %s\n", e.what());
        remove_from_list(r);
        l.lock();
        impl_ = NULL;
        throw;
    }
    remove_from_list(r);
    l.lock();
    impl_ = NULL;
    return NULL;
}

void runner::add_waiter(task &t) {
    m->s->add_waiter(t);
}

thread runner::get_thread() { return thread(m->tt); }

task runner::get_task() {
    return m->s->get_current_task();
}

scheduler &runner::get_scheduler() { return *(m->s); }

void runner::swap_to_scheduler() {
    impl_->s->swap_to_scheduler();
}

unsigned long runner::count() {
    mutex::scoped_lock l(*tmutex);
    return runners->size();
}

void runner::set_thread_timeout(unsigned int ms) {
    mutex::scoped_lock l(*tmutex);
    thread_timeout_ms = ms;
}


static void backtrace_handler(int sig_num, siginfo_t *info, void *ctxt) {
    // http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes
    // TODO: maybe use the google logging demangler that doesn't alloc
    hack::ucontext *uc = (hack::ucontext *)ctxt;

    // Get the address at the time the signal was raised from the instruction pointer
#if __i386__
    void *caller_address = (void *) uc->uc_mcontext.eip;
#elif __amd64__
    void *caller_address = (void *) uc->uc_mcontext.rip;
#else
    #error "arch not supported"
#endif

    fprintf(stderr, "signal %d (%s), address is %p from %p\n",
        sig_num, strsignal(sig_num), info->si_addr,
        (void *)caller_address);

    void *array[50];
    int size = backtrace(array, 50);

    // overwrite sigaction with caller's address
    array[1] = caller_address;

    char **messages = backtrace_symbols(array, size);

    // skip first stack frame (points here)
    for (int i = 1; i < size && messages != NULL; ++i) {
        char *mangled_name = 0, *offset_begin = 0, *offset_end = 0;

        // find parantheses and +address offset surrounding mangled name
        for (char *p = messages[i]; *p; ++p) {
            if (*p == '(') {
                mangled_name = p;
            } else if (*p == '+') {
                offset_begin = p;
            } else if (*p == ')') {
                offset_end = p;
                break;
            }
        }

        // if the line could be processed, attempt to demangle the symbol
        if (mangled_name && offset_begin && offset_end &&
            mangled_name < offset_begin)
        {
            *mangled_name++ = '\0';
            *offset_begin++ = '\0';
            *offset_end++ = '\0';

            int status;
            char * real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

            if (status == 0) {
                // if demangling is successful, output the demangled function name
                std::cerr << "[bt]: (" << i << ") " << messages[i] << " : "
                    << real_name << "+" << offset_begin << offset_end
                    << std::endl;

            } else {
                // otherwise, output the mangled function name
                std::cerr << "[bt]: (" << i << ") " << messages[i] << " : "
                    << mangled_name << "+" << offset_begin << offset_end
                    << std::endl;
            }
            free(real_name);
        } else {
            // otherwise, print the whole line
            std::cerr << "[bt]: (" << i << ") " << messages[i] << std::endl;
        }
    }
    std::cerr << std::endl;

    free(messages);

    exit(EXIT_FAILURE);
}

void runner::init() {
    mutex::scoped_lock l(*tmutex);
    if (impl_ == NULL) {
        ncpu();
        // create alternate stack for signal handlers
        stack_t ss;
        ss.ss_sp = malloc(SIGSTKSZ);
        ss.ss_size = SIGSTKSZ;
        ss.ss_flags = 0;
        THROW_ON_ERROR(sigaltstack(&ss, NULL));

        struct sigaction act;
        memset(&act, 0, sizeof(act));
        // install SIGSEGV handler
        act.sa_sigaction = backtrace_handler;
        act.sa_flags = SA_RESTART | SA_SIGINFO;
        THROW_ON_ERROR(sigaction(SIGSEGV, &act, NULL));
        // install SIGABRT handler
        act.sa_sigaction = backtrace_handler;
        act.sa_flags = SA_RESTART | SA_SIGINFO;
        THROW_ON_ERROR(sigaction(SIGABRT, &act, NULL));
        // ignore SIGPIPE
        THROW_ON_ERROR(sigaction(SIGPIPE, NULL, &act));
        if (act.sa_handler == SIG_DFL) {
            act.sa_handler = SIG_IGN;
            THROW_ON_ERROR(sigaction(SIGPIPE, &act, NULL));
        }
        runner::shared_impl i(new impl);
        runner r = i->to_runner(l);
        append_to_list(r, l);
        impl_ = i.get();
    }
}

int runner::main() {
    mutex::scoped_lock l(*tmutex);
    runner r = impl_->to_runner(l);
    l.unlock();
    r.schedule();
    return 0;
}

