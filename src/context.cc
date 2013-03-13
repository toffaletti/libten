#include "context.hh"

namespace ten {

context::context() noexcept : _ctx{_os_ctx.get()} {}

//! make a new context and stack
context::context(func_type f, size_t stack_size) {
    void *stack = allocator.allocate(stack_size);
#ifndef NVALGRIND
    valgrind_stack_id =
        VALGRIND_STACK_REGISTER(stack, reinterpret_cast<intptr_t>(stack)-stack_size);
#endif
    _ctx = boost::context::make_fcontext(stack, stack_size, f);
}

context::~context() {
    if (_ctx->fc_stack.sp) {
        void *stack = _ctx->fc_stack.sp;
#ifndef NVALGRIND
        VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
        allocator.deallocate(stack, stack_size());
    }
}

intptr_t context::swap(context &other, intptr_t arg) noexcept {
    return boost::context::jump_fcontext(_ctx,
            other._ctx,
            arg);
}

size_t context::stack_size() const {
    return _ctx->fc_stack.size;
}

} // ten
