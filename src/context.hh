#ifndef LIBTEN_TASK2_CONTEXT_HH
#define LIBTEN_TASK2_CONTEXT_HH

#include <boost/context/fcontext.hpp>
#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

#include "ten/logging.hh"

class fast_stack_allocator {
public:
    void *allocate(size_t stack_size) {
        void *stack_end = nullptr;
        int r = posix_memalign(&stack_end, 4096, stack_size);
        DCHECK(r == 0);
        DCHECK(stack_end);
        stack_end = reinterpret_cast<void *>(
                reinterpret_cast<intptr_t>(stack_end) + stack_size);
        return stack_end;
    }

    void deallocate(void *p, size_t stack_size) {
        free(reinterpret_cast<void *>(
                    reinterpret_cast<intptr_t>(p)-stack_size));
    }
};

class context {
private:
    //boost::ctx::stack_allocator allocator;
    fast_stack_allocator allocator;
private:
    boost::ctx::fcontext_t _ctx;
#ifndef NVALGRIND
    //! stack id so valgrind doesn't freak when stack swapping happens
    int valgrind_stack_id;
#endif
public:
    typedef void (*func_type)(intptr_t);
public:
    context(const context &) = delete;
    context(const context &&) = delete;

    //! make context for existing stack
    context() noexcept {}

    //! make a new context and stack
    explicit context(func_type f) {
        size_t stacksize = boost::ctx::default_stacksize();
        void *stack = allocator.allocate(stacksize);
#ifndef NVALGRIND
        valgrind_stack_id =
            VALGRIND_STACK_REGISTER(stack, reinterpret_cast<intptr_t>(stack)-stacksize);
#endif
        _ctx.fc_stack.base = stack;
        _ctx.fc_stack.limit = reinterpret_cast<void *>(reinterpret_cast<intptr_t>(stack)-stacksize);
        boost::ctx::make_fcontext(&_ctx, f);
    }

    intptr_t swap(context &other, intptr_t arg=0) noexcept {
        return boost::ctx::jump_fcontext(&_ctx,
                &other._ctx,
                arg);
    }

    ~context() {
        if (_ctx.fc_stack.base) {
            size_t stacksize = boost::ctx::default_stacksize();
            void *stack = _ctx.fc_stack.base;
#ifndef NVALGRIND
            VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
            allocator.deallocate(stack, stacksize);
        }
    }
};



#endif

