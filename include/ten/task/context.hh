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

#if USE_BOOST_FCONTEXT
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
