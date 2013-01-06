#ifndef LIBTEN_TASK2_CONTEXT_HH
#define LIBTEN_TASK2_CONTEXT_HH

#include <boost/context/fcontext.hpp>

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

constexpr size_t default_stacksize() {
    return 256*1024;
}

class context {
private:
    //boost::ctx::stack_allocator allocator;
    fast_stack_allocator allocator;
private:
    boost::context::fcontext_t *_ctx;
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
    context(boost::context::fcontext_t &ctx) noexcept : _ctx{&ctx} {}

    //! make a new context and stack
    explicit context(func_type f) {
        size_t stacksize = default_stacksize();
        void *stack = allocator.allocate(stacksize);
#ifndef NVALGRIND
        valgrind_stack_id =
            VALGRIND_STACK_REGISTER(stack, reinterpret_cast<intptr_t>(stack)-stacksize);
#endif
        _ctx = boost::context::make_fcontext(stack, stacksize, f);
    }

    intptr_t swap(context &other, intptr_t arg=0) noexcept {
        return boost::context::jump_fcontext(_ctx,
                other._ctx,
                arg);
    }

    ~context() {
        if (_ctx->fc_stack.sp) {
            size_t stacksize = _ctx->fc_stack.size;
            void *stack = _ctx->fc_stack.sp;
#ifndef NVALGRIND
            VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
            allocator.deallocate(stack, stacksize);
        }
    }
};



#endif

