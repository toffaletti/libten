#include <boost/context/fcontext.hpp>
#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>
#include <functional>

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

class coroutine;

namespace this_coro {
    void yield() noexcept;
    bool yield_to(coroutine &other) noexcept;
}

class coroutine {
private:
    static boost::ctx::stack_allocator allocator;
private:
    coroutine *_parent = nullptr;
    boost::ctx::fcontext_t _ctx;
    std::function<void ()> _f;
#ifndef NVALGRIND
    //! stack id so valgrind doesn't freak when stack swapping happens
    int valgrind_stack_id;
#endif

    friend void this_coro::yield() noexcept;
    friend bool this_coro::yield_to(coroutine &other) noexcept;
public:
    coroutine(const coroutine&) = delete;
    coroutine(const coroutine &&) = delete;

    //! setup existing stack as coroutine
    coroutine() noexcept;

    //! create a new coroutine
    template<class Function, class... Args> 
    explicit coroutine(Function &&f, Args&&... args) {
        size_t stacksize = boost::ctx::default_stacksize();
        void *stack = allocator.allocate(stacksize);
#ifndef NVALGRIND
        valgrind_stack_id =
            VALGRIND_STACK_REGISTER(stack, reinterpret_cast<intptr_t>(stack)-stacksize);
#endif
        _f = std::bind(f, args...);
        _ctx.fc_stack.base = stack;
        _ctx.fc_stack.limit = reinterpret_cast<void *>(reinterpret_cast<intptr_t>(stack)-stacksize);
        boost::ctx::make_fcontext(&_ctx, entry);
    }

    ~coroutine() {
        if (_ctx.fc_stack.base) {
            size_t stacksize = boost::ctx::default_stacksize();
            void *stack = _ctx.fc_stack.base;
#ifndef NVALGRIND
            VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
            allocator.deallocate(stack, stacksize);
        }
    }

    void join() noexcept;

private:
    static void entry(intptr_t arg) {
        coroutine *self = reinterpret_cast<coroutine *>(arg);
        self->execute();
    }

    void execute() {
        _f();
        _f = nullptr;
        yield();
    }

protected:
    bool yield_to(coroutine &other) noexcept;
    void yield() noexcept;
};

