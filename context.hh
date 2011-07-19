#ifndef CONTEXT_HH
#define CONTEXT_HH

#ifdef USE_UCONTEXT
#include <ucontext.h>
struct context {
    typedef void (*proc)(void *);
    ucontext_t uc;

    void init(proc f=0, void *arg=0, char *stack=0, size_t stack_size=0) {
        getcontext(&uc);
        if (f && stack && stack_size) {
            uc.uc_stack.ss_sp = stack;
            uc.uc_stack.ss_size = stack_size;
            uc.uc_link = 0;
            // this depends on glibc's work around that allows
            // pointers to be passed to makecontext
            // it also depends on g++ allowing static C++
            // functions to be used for C linkage
            makecontext(&uc, (void (*)())f, 1, arg);
        }
    }

    void swap(context *other) {
        swapcontext(&uc, &other->uc);
    }
};
#else
#error "no context implementation chosen"
#endif

#endif // CONTEXT_HH

