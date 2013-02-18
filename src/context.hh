#ifndef LIBTEN_TASK_CONTEXT_HH
#define LIBTEN_TASK_CONTEXT_HH

#include <boost/context/fcontext.hpp>
#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>

#include <sys/mman.h>

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

#include "ten/logging.hh"

namespace ten {

class stack_allocator {
public:
    static constexpr size_t page_size = 4096;

    void *allocate(size_t stack_size) noexcept {
        void *stack_end = nullptr;
        size_t real_size = stack_size + page_size;
        int r = posix_memalign(&stack_end, page_size, real_size);
        CHECK(r == 0) << "could not allocate stack " << r;
        // protect the guard page
        PCHECK(mprotect(stack_end, page_size, PROT_NONE) == 0);
        stack_end = reinterpret_cast<void *>(
                reinterpret_cast<intptr_t>(stack_end) + stack_size + page_size);
        return stack_end;
    }

    void deallocate(void *p, size_t stack_size) noexcept {
        void *stack_end = reinterpret_cast<void *>(
                reinterpret_cast<intptr_t>(p) - stack_size - page_size);
        PCHECK(mprotect(stack_end, page_size, PROT_READ|PROT_WRITE) == 0);
        free(stack_end);
    }
};

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
        init(f, stack_size);
    }

    void init(func_type f, size_t stack_size = boost::ctx::default_stacksize()) {
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

