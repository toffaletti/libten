#ifndef LIBTEN_TASK2_COROUTINE_HH
#define LIBTEN_TASK2_COROUTINE_HH

#include <boost/context/fcontext.hpp>
#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>
#include <functional>

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

class coroutine;

namespace this_coro {
    coroutine *get() noexcept;
    void yield() noexcept;
    bool yield_to(coroutine &other) noexcept;
}

struct coroutine_cancel {};

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
    bool done() {
        // main coro will have no parent
        // coro that hasn't run yet will have no parent
        if (_parent == nullptr) return false;
        // we still have a function to run
        if (_f) return false;
        return true;
    }

private:
    static void entry(intptr_t arg) {
        coroutine *self = reinterpret_cast<coroutine *>(arg);
        self->execute();
    }

    void execute() {
        try {
            _f();
        } catch (coroutine_cancel &e) {}
        _f = nullptr;
        yield();
    }

public:
    bool yield_to(coroutine &other) noexcept;
protected:
    void yield() noexcept;
};

#endif // LIBTEN_TASK2_COROUTINE_HH

