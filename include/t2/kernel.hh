#ifndef TEN_KERNEL_HH
#define TEN_KERNEL_HH

#include "ten/logging.hh"
#include "ten/descriptors.hh"
#include <atomic>
#include <sys/syscall.h> // gettid
#include <sys/stat.h> // umask
#include <signal.h> // sigaction

namespace t2 {
class kernel;

extern kernel *the_kernel;

class kernel {
public:
    std::atomic<bool> shutdown;
    std::atomic<uint64_t> taskcount;
    ten::signal_fd sigfd; // TODO: this is only here because of stupid init order. needs to happen before any threads
public:
    kernel(const kernel&) = delete;
    kernel &operator =(const kernel &) = delete;

    kernel() :
        shutdown{false},
        taskcount{0}
    {
        CHECK(the_kernel == nullptr);
        the_kernel = this;
        _init();
    }

    ~kernel() {
    }

private:
    void _init() {
        using namespace ten;
        // allow log files and message queues to be created group writable
        umask(0);
        InitGoogleLogging(program_invocation_short_name);
        InstallFailureSignalHandler();
        FLAGS_logtostderr = true;


        CHECK(getpid() == syscall(SYS_gettid)) << "must call in main thread before anything else";
        //ncpu_ = sysconf(_SC_NPROCESSORS_ONLN);
        stack_t ss;
        ss.ss_sp = calloc(1, SIGSTKSZ);
        ss.ss_size = SIGSTKSZ;
        ss.ss_flags = 0;
        PCHECK(sigaltstack(&ss, NULL) == 0) << "setting signal stack failed";

        struct sigaction act;

        // ignore SIGPIPE
        memset(&act, 0, sizeof(act));
        PCHECK(sigaction(SIGPIPE, NULL, &act) == 0) << "getting sigpipe handler failed";
        if (act.sa_handler == SIG_DFL) {
            act.sa_handler = SIG_IGN;
            PCHECK(sigaction(SIGPIPE, &act, NULL) == 0) << "setting sigpipe handler failed";
        }

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGQUIT);
        sigfd = ten::signal_fd(mask);
        //netinit();
    }
};

} // t2

#endif

