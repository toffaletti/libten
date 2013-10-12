#include "context.hh"

namespace ten {

context::context() noexcept : _ctx{_os_ctx.get()} {}

//! make a new context and stack
context::context(func_type f, size_t stack_size) {
    void *stack = stack_allocator::allocate(stack_size);
#ifndef NVALGRIND
    valgrind_stack_id =
        VALGRIND_STACK_REGISTER(stack, reinterpret_cast<intptr_t>(stack)-stack_size);
#endif

#if BOOST_VERSION == 105100
    _ctx = new ctx::fcontext_t();
    _ctx->fc_stack.base = stack;
    _ctx->fc_stack.limit = reinterpret_cast<void *>(reinterpret_cast<intptr_t>(stack)-stack_size);
    ctx::make_fcontext(_ctx, f);
#elif BOOST_VERSION >= 105200
    _ctx = ctx::make_fcontext(stack, stack_size, f);
#endif
}

context::~context() {
#if BOOST_VERSION == 105100
    if (_ctx->fc_stack.base) {
        void *stack = _ctx->fc_stack.base;
#ifndef NVALGRIND
        VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
        stack_allocator::deallocate(stack, stack_size());
        delete _ctx;
    }

#elif BOOST_VERSION >= 105200
    if (_ctx->fc_stack.sp) {
        void *stack = _ctx->fc_stack.sp;
#ifndef NVALGRIND
        VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
        stack_allocator::deallocate(stack, stack_size());
    }
#endif
}

intptr_t context::swap(context &other, intptr_t arg) noexcept {
    return ctx::jump_fcontext(_ctx,
            other._ctx,
            arg);
}

size_t context::stack_size() const {
#if BOOST_VERSION == 105100
    return reinterpret_cast<intptr_t>(_ctx->fc_stack.base) -
        reinterpret_cast<intptr_t>(_ctx->fc_stack.limit);
#elif BOOST_VERSION >= 105200
    return _ctx->fc_stack.size;
#endif
}

} // ten
