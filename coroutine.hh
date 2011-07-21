#ifndef COROUTINE_HH
#define COROUTINE_HH
#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <vector>
#include <sys/mman.h>
#include "context.hh"
#include "error.hh"

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

class coroutine : boost::noncopyable {
public:
    typedef void (*proc)(void *);

    coroutine() : stack(0), stack_size(0) { ctxt.init(); }

    coroutine(proc f, void *arg=NULL, size_t stack_size_=4096)
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

    ~coroutine() {
        if (stack) {
#ifndef NVALGRIND
            VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
            THROW_ON_ERROR(mprotect(stack+stack_size, getpagesize(), PROT_READ|PROT_WRITE));
            free(stack);
        }
    }

    void swap(coroutine *to) {
        ctxt.swap(&to->ctxt);
    }

    bool main() { return stack == 0; }

private:
    context ctxt;
#ifndef NVALGRIND
    int valgrind_stack_id;
#endif
    char *stack;
    // TODO: probably not needed, can be retrieved from context?
    size_t stack_size;
};

#endif // COROUTINE_HH