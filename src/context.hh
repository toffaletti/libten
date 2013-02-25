#ifndef LIBTEN_TASK_CONTEXT_HH
#define LIBTEN_TASK_CONTEXT_HH

#include "stack_alloc.hh"
#include <boost/context/fcontext.hpp>

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

namespace ten {

class context {
private:
    stack_allocator allocator;
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
    explicit context(func_type f, size_t stack_size = boost::ctx::default_stacksize()) {
        memset(&_ctx, 0, sizeof(_ctx));
        void *stack = allocator.allocate(stack_size);
#ifndef NVALGRIND
        valgrind_stack_id =
            VALGRIND_STACK_REGISTER(stack, reinterpret_cast<intptr_t>(stack)-stack_size);
#endif
        _ctx.fc_stack.base = stack;
        _ctx.fc_stack.limit = reinterpret_cast<void *>(reinterpret_cast<intptr_t>(stack)-stack_size);
        boost::ctx::make_fcontext(&_ctx, f);
    }

    intptr_t swap(context &other, intptr_t arg=0) noexcept {
        return boost::ctx::jump_fcontext(&_ctx,
                &other._ctx,
                arg);
    }

    size_t stack_size() const {
        return reinterpret_cast<intptr_t>(_ctx.fc_stack.base) -
            reinterpret_cast<intptr_t>(_ctx.fc_stack.limit);
    }

    ~context() {
        if (_ctx.fc_stack.base) {
            void *stack = _ctx.fc_stack.base;
#ifndef NVALGRIND
            VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
            allocator.deallocate(stack, stack_size());
        }
    }
};

} // ten

#endif

