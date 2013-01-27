#ifndef TEN_KERNEL_HH
#define TEN_KERNEL_HH

#include <atomic>
#include "ten/logging.hh"
#include <sys/syscall.h> // gettid
#include <sys/stat.h> // umask
#include <signal.h> // sigaction

namespace ten {
class kernel;

extern kernel the_kernel;

class kernel {
public:
    std::atomic<bool> shutdown;
    std::atomic<uint64_t> taskcount;
public:
    kernel(const kernel&) = delete;
    kernel &operator =(const kernel &) = delete;

    kernel() :
        shutdown{false},
        taskcount{0}
    {
        _init();
    }

    ~kernel() {
    }

private:
    static void _init() {
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

        //netinit();
    }
};

} // ten

#endif

