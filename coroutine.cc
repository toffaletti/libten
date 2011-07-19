#include "coroutine.hh"
#include "error.hh"

// setjmp/longjmp:
// http://stackoverflow.com/questions/4352451/coroutine-demo-source-2

// http://mfichman.blogspot.com/2011/05/lua-style-coroutines-in-c.html

coroutine::coroutine()
    : stack(0), stack_size(0)
{
    ctxt.init();
}

coroutine::coroutine(proc f, void *arg, size_t stack_size_)
    : stack_size(stack_size_)
{
    // add on size for a guard page
    size_t real_size = stack_size + getpagesize();
    int r = posix_memalign((void **)&stack, getpagesize(), real_size);
    THROW_ON_NONZERO(r);
    // protect the guard page
    THROW_ON_ERROR(mprotect(stack+stack_size, getpagesize(), PROT_NONE));
#ifndef NVALGRIND
    valgrind_stack_id =
        VALGRIND_STACK_REGISTER(stack, stack+stack_size);
#endif
    ctxt.init(f, arg, stack, stack_size);
}

coroutine::~coroutine() {
    if (stack) {
#ifndef NVALGRIND
        VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
        THROW_ON_ERROR(mprotect(stack+stack_size, getpagesize(), PROT_READ|PROT_WRITE));
        free(stack);
    }
}

void coroutine::swap(coroutine *to) {
    ctxt.swap(&to->ctxt);
}

