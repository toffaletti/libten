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

    void init(proc f=nullptr, void *arg=nullptr, char *stack=nullptr, size_t stack_size=0) {
        getcontext(this);
        if (f && stack && stack_size) {
            uc_stack.ss_sp = stack;
            uc_stack.ss_size = stack_size;
            uc_link = nullptr;
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
#include <boost/context/fcontext.hpp>

namespace ten {

struct context : boost::ctx::fcontext_t {
    typedef void (*proc)(void *);
    typedef void (*proc_ctx)(intptr_t);
    intptr_t arg;

    void init(proc f=nullptr, void *arg_=nullptr, char *stack=nullptr, size_t stack_size=0) {
        memset(this, 0, sizeof(context));
        arg = (intptr_t)arg_;
        if (f && stack && stack_size) {
            fc_stack.base = stack;
            // stack grows down
            fc_stack.limit = stack-stack_size;
            boost::ctx::make_fcontext(this, (proc_ctx)f);
        }
    }

    void swap(context *other) {
        boost::ctx::jump_fcontext(this, other, other->arg);
    }
};

} // end namespace ten 

#else
#error "no context implementation chosen"
#endif

#endif // LIBTEN_TASK_CONTEXT_HH
