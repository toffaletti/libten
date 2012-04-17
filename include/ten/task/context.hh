#ifndef LIBTEN_TASK_CONTEXT_HH
#define LIBTEN_TASK_CONTEXT_HH

//! \file
//! context handles switching stacks
//
//! there are two implementations, ucontext
//! and fcontext. ucontext is slower because it
//! also saves and restores the signal handler state
//! this requires system calls. fcontext only saves
//! the bare minimum register states.

#ifdef USE_UCONTEXT
#include <ucontext.h>

namespace ten { 

struct context : ucontext_t {
    typedef void (*proc)(void *);

    void init(proc f=0, void *arg=0, char *stack=0, size_t stack_size=0) {
        getcontext(this);
        if (f && stack && stack_size) {
            uc_stack.ss_sp = stack;
            uc_stack.ss_size = stack_size;
            uc_link = 0;
            // this depends on glibc's work around that allows
            // pointers to be passed to makecontext
            // it also depends on g++ allowing static C++
            // functions to be used for C linkage
            makecontext(this, (void (*)())f, 1, arg);
        }
    }

    void swap(context *other) {
        swapcontext(this, other);
    }
};

} // end namespace ten

#elif USE_BOOST_FCONTEXT
#include <string.h>
#include "boost/context/fcontext.hpp"

namespace ten {

struct context : boost_fcontext_t {
    typedef void (*proc)(void *);

    void init(proc f=0, void *arg=0, char *stack=0, size_t stack_size=0) {
        memset(this, 0, sizeof(context));
        if (f && stack && stack_size) {
            fc_stack.ss_base = stack;
            // stack grows down
            fc_stack.ss_limit = stack-stack_size;
            boost_fcontext_make(this, f, arg);
        }
    }

    void swap(context *other) {
        boost_fcontext_jump(this, other, 0);
    }
};

} // end namespace ten 

#else
#error "no context implementation chosen"
#endif

#endif // LIBTEN_TASK_CONTEXT_HH
